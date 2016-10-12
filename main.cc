#include "boost_algs.h"

#include <dynograph_util.hh>
#include "distributed_dataset.h"
#include <hooks.h>

#include <chrono>
#include <sstream>
#include <vector>

using std::cout;
using std::cerr;
using std::ifstream;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::string;
using std::unique_ptr;
using std::vector;
using std::stringstream;

void printTime(string stepDesc, duration<double, std::milli> diff)
{
    cerr << stepDesc << ": " << diff.count() << " ms.\n";
}

void insertBatch(DynoGraph::Batch& batch, Graph &g, Graph::vertices_size_type max_nv)
{
    for (DynoGraph::Edge e : batch)
    {
        assert(e.src > 0 && e.dst > 0);
        assert(static_cast<Graph::vertices_size_type>(e.src) < max_nv);
        assert(static_cast<Graph::vertices_size_type>(e.dst) < max_nv);
        Hooks::getInstance().traverse_edge();
        VertexId Src = boost::vertex(e.src, g);
        VertexId Dst = boost::vertex(e.dst, g);
        // Try to insert the edge
        std::pair<Edge, bool> inserted_edge = boost::add_edge(
                Src, Dst,
                // Boost uses template nesting to implement multiple edge properties
                Weight(e.weight, Timestamp(e.timestamp)),
                g);
        // If the edge already existed...
        if (!inserted_edge.second) {
            Edge &existing_edge = inserted_edge.first;
            // Increment edge weight
            int64_t weight = get(boost::edge_weight, g, existing_edge);
            weight += e.weight;
            put(boost::edge_weight, g, existing_edge, weight);
            // Overwrite timestamp
            put(boost::edge_timestamp, g, existing_edge, e.timestamp);
        }

    }
}

void deleteEdges(int64_t threshold, Graph &g)
{
    // TODO Make sure this is actually running in parallel
    boost::remove_edge_if(
        [&](const Edge &e)
        {
            return get(boost::edge_timestamp, g, e) < threshold;
        }
    ,g);
}

void reportOwnership(Graph &g)
{
    int64_t edgeCount = 0;
    int64_t myEdgeCount = 0;
    auto allEdges = edges(g);
    for (auto e = allEdges.first; e != allEdges.second; ++e)
    {
        if (e->owner() == g.process_group().rank)
        {
            edgeCount += 1;
        }
        myEdgeCount += 1;
    }

    cerr << "P" << g.process_group().rank
         << ": total edges = " << edgeCount
         << " my edges = " << myEdgeCount << std::endl;
    synchronize(g.process_group());
}


// Helper functions to split strings
// http://stackoverflow.com/a/236803/1877086
void split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
}
vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}


int main(int argc, char *argv[]) {
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    ProcessGroup pg;

    // Don't flush output after each operation. Helps keep per-process output together
    cerr << std::nounitbuf;

    // Parse the command line
    DynoGraph::Args args(argc, argv);

    // Pre-load the edge batches
    if (process_id(pg) == 0) { cerr << DynoGraph::msg << "Pre-loading " << args.input_path << " from disk...\n"; }
    auto t1 = steady_clock::now();

    unique_ptr<DynoGraph::Dataset> dataset = DynoGraph::loadDatasetDistributed(args, communicator(pg));

    auto t2 = steady_clock::now();
    if (process_id(pg) == 0) { printTime("Graph pre-load", t2 - t1); }

    // Create the graph
    // TODO use a round-robin distribution strategy, then over-provision based on available memory|
    Graph::vertices_size_type max_num_vertices = static_cast<Graph::vertices_size_type>(dataset->getMaxNumVertices());
    Graph g(max_num_vertices, pg);

    Hooks& hooks = Hooks::getInstance();

    for (int64_t trial = 0; trial < args.num_trials; ++trial)
    {
        hooks.trial = trial;
        // Ingest each batch and run analytics
        for (int batchId = 0; batchId < args.num_batches; ++batchId)
        {
            hooks.batch = batchId;

            hooks.region_begin("preprocess");
            DynoGraph::Batch& batch = *dataset->getBatch(batchId);
            hooks.region_end("preprocess");

            // Deletions
            if (args.enable_deletions)
            {
                int64_t modified_after = dataset->getTimestampForWindow(batchId);
                if (process_id(pg) == 0) { cerr << DynoGraph::msg << "Deleting edges older than " << modified_after << "\n"; }
                hooks.region_begin("deletions");
                deleteEdges(modified_after, g);
                hooks.region_end("deletions");

                synchronize(pg);
            }

            // Batch insertion
            if (process_id(pg) == 0) { cerr << "Loading batch " << batchId << "...\n"; }
            hooks.region_begin("insertions");
            insertBatch(batch, g, max_num_vertices);
            hooks.region_end("insertions");

            synchronize(pg);

            cout << "{\"pid\":"         << process_id(pg) << ","
                 << "\"num_vertices\":" << num_vertices(g) << ","
                 << "\"num_edges\":"    << num_edges(g) << "}\n";

            // Algorithm
            for (string algName : split(args.alg_name, ' '))
            {
                runAlgorithm(algName, g, max_num_vertices);
                synchronize(pg);
            }
        }
    }

    return 0;
}
