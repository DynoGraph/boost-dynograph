#include "boost_dynamic_graph.h"
#include <dynograph_util/dynograph_impl_test.h>

INSTANTIATE_TYPED_TEST_CASE_P(BOOST_DYNOGRAPH, ImplTest, boost_dynamic_graph);

// This test won't work with dynograph_util in MPI mode,
// because the reference_impl's in the other processes will be passed  an empty batch
// INSTANTIATE_TYPED_TEST_CASE_P(BOOST_DYNOGRAPH, CompareWithReferenceTest, boost_dynamic_graph);

TEST(BOOST_DYNOGRAPH, ScatterBatchTest)
{
    DynoGraph::Args args;
    args.input_path = "dynograph_util/data/worldcup-10K.graph.bin";
    args.batch_size = 44500;
    args.num_epochs = 1;
    args.sort_mode = DynoGraph::Args::SORT_MODE::UNSORTED;
    args.num_trials = 1;
    args.num_alg_trials = 1;
    args.window_size = 1.0;
    DynoGraph::EdgeListDataset dataset(args);
    Graph graph(dataset.getMaxVertexId() + 1);

    auto batch = dataset.getBatch(0);
    std::vector<edge_update> local_updates;
    boost_dynamic_graph::scatter_batch(graph, *batch, local_updates);

    auto comm = boost::mpi::communicator();

    // Make sure all updates ended up somewhere
    size_t global_num_updates = 0;
    boost::mpi::reduce(comm, local_updates.size(), global_num_updates, std::plus<size_t>(), 0);
    if (comm.rank() == 0) {
        EXPECT_EQ(batch->size(), global_num_updates);
    }

    // Make sure all the updates in this vertex belong to this rank
    for (auto u : local_updates)
    {
        BoostVertex v = boost::vertex(u.src, graph);
        EXPECT_EQ(v.owner, comm.rank());
    }

}

int main(int argc, char **argv)
{
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
