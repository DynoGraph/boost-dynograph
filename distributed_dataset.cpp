//
// Created by ehein6 on 8/1/16.
//

#include "distributed_dataset.h"
#include <boost/mpi/collectives/scatter.hpp>

using namespace DynoGraph;
using std::vector;

std::unique_ptr<Dataset>
DynoGraph::loadDatasetDistributed(std::string path, int64_t numBatches, boost::mpi::communicator comm)
{
    // Rank 0 loads the whole dataset first
    std::unique_ptr<Dataset> dataset;
    int64_t max_nv;
    if (comm.rank() == 0)
    {
        dataset = std::unique_ptr<Dataset>(new Dataset(path, numBatches));
        max_nv = dataset->getMaxNumVertices();
    }
    boost::mpi::broadcast(comm, max_nv, 0);

    /*
     * Now We need to distribute the Dataset to each process in the group
     * Each process will have the same number of batches, but each batch
     * will be a slice of the corresponding batch in the parent
     */
    //
    vector<Edge> myEdges; // TODO reserve storage in advance
    for (int batchId = 0; batchId < numBatches; ++batchId)
    {
        vector<vector<Edge>> dividedBatch(comm.size());
        if (comm.rank() == 0)
        {
            // Divide the batch into slices, one per process
            size_t edgesPerBatch = dataset->batches[0].end() - dataset->batches[0].begin();
            size_t edgesPerRank = edgesPerBatch / comm.size(); // TODO round up here
            Batch & thisBatch = dataset->batches[batchId];
            for (int i = 0; i < comm.size(); ++i)
            {
                size_t offset = i * edgesPerRank;
                auto begin = thisBatch.begin() + offset;
                auto end = begin + edgesPerRank;
                // Give the remainder to the last rank
                if (i == comm.size()-1) { end = thisBatch.end(); }
                dividedBatch[i] = vector<Edge>(begin, end); // TODO Move construct here
            }
        }
        // Scatter the slices to the processes
        vector<Edge> myBatch; // TODO round up here, and reserve space
        boost::mpi::scatter(comm, dividedBatch, myBatch, 0);

        // Append to my list of edges
        myEdges.insert(myEdges.end(), myBatch.begin(), myBatch.end());
    }

    // Finally we construct a dataset for this process using the local set of edges
    return std::unique_ptr<Dataset>(new Dataset(myEdges, numBatches, max_nv));
}