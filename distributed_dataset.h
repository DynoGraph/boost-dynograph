//
// Created by ehein6 on 8/1/16.
//

#ifndef BOOST_DYNOGRAPH_DISTRIBUTEDDATASET_H
#define BOOST_DYNOGRAPH_DISTRIBUTEDDATASET_H

#include <dynograph_util.hh>
#include <boost/graph/use_mpi.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/serialization/serialization.hpp>
#include <memory>

BOOST_IS_MPI_DATATYPE(DynoGraph::Edge)
BOOST_IS_BITWISE_SERIALIZABLE(DynoGraph::Edge)

namespace DynoGraph
{
std::unique_ptr<Dataset>
loadDatasetDistributed(std::string path, int64_t numBatches, boost::mpi::communicator comm);
}
#endif //BOOST_DYNOGRAPH_DISTRIBUTEDDATASET_H
