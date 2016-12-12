//
// Created by ehein6 on 8/1/16.
//

#ifndef BOOST_DYNOGRAPH_DISTRIBUTEDDATASET_H
#define BOOST_DYNOGRAPH_DISTRIBUTEDDATASET_H

#include <dynograph_util.h>
#include <boost/graph/use_mpi.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/serialization/serialization.hpp>
#include <memory>

BOOST_IS_MPI_DATATYPE(DynoGraph::Edge)
BOOST_IS_BITWISE_SERIALIZABLE(DynoGraph::Edge)

namespace DynoGraph
{
std::unique_ptr<Dataset>
loadDatasetDistributed(DynoGraph::Args args, boost::mpi::communicator comm);
}
#endif //BOOST_DYNOGRAPH_DISTRIBUTEDDATASET_H
