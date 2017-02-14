#include "boost_dynamic_graph.h"
#include <dynograph_impl_test.h>

INSTANTIATE_TYPED_TEST_CASE_P(BOOST_DYNOGRAPH, ImplTest, boost_dynamic_graph);
INSTANTIATE_TYPED_TEST_CASE_P(BOOST_DYNOGRAPH, CompareWithReferenceTest, boost_dynamic_graph);

int main(int argc, char **argv)
{
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
