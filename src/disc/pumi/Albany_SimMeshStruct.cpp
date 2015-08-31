//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#include "Albany_SimMeshStruct.hpp"

#include <MeshSim.h>
#include <SimPartitionedMesh.h>
#include <gmi_sim.h>
#include <SimUtil.h>
#include <SimField.h>
#include <apfSIM.h>
#include <PCU.h>

Albany::SimMeshStruct::SimMeshStruct(
    const Teuchos::RCP<Teuchos::ParameterList>& params,
		const Teuchos::RCP<const Teuchos_Comm>& commT)
{
  SimUtil_start();
  Sim_readLicenseFile(0);
  MS_init();
  SimPartitionedMesh_start(NULL, NULL);
  gmi_sim_start();
  gmi_register_sim();
  PCU_Comm_Init();

  params->validateParameters(
      *(SimMeshStruct::getValidDiscretizationParameters()), 0);

  outputFileName = params->get<std::string>("Sim Output File Name", "");
  outputInterval = params->get<int>("Sim Write Interval", 1); // write every time step default

  std::string native_file;
  if (params->isParameter("Acis Model Input File Name"))
    native_file = params->get<std::string>("Parasolid Model Input File Name");
  if(params->isParameter("Parasolid Model Input File Name"))
    native_file = params->get<std::string>("Parasolid Model Input File Name");
  std::string smd_file;
  if(params->isParameter("Sim Model Input File Name"))
    smd_file = params->get<std::string>("Sim Model Input File Name");

  model = gmi_sim_load(native_file.empty() ? 0 : native_file.c_str(),
                       smd_file.empty()    ? 0 : smd_file.c_str());
  pGModel sim_model = gmi_export_sim(model);

  std::string mesh_file = params->get<std::string>("Sim Input File Name");
  pParMesh sim_mesh = PM_load(mesh_file.c_str(), sthreadNone, sim_model, NULL);
  mesh = apf::createMesh(sim_mesh);

  APFMeshStruct::init(params, commT);

  if (params->isParameter("Sim Restart File Name")) {
    std::cerr << "reading solution from file!\n";
    hasRestartSolution = true;
    assert(!params->isParameter("Solution Vector Components"));
    std::string field_file = params->get<std::string>("Sim Restart File Name");
    pField sim_field = Field_load(field_file.c_str(), sim_mesh, 0, 0);
    apf::Field* field = apf::wrapSIMField(mesh, sim_field);
    std::string name = apf::getName(field);
    if (name != Albany::APFMeshStruct::solution_name) {
      std::cerr << "renaming restart field \"" << name << "\" to \""
        << Albany::APFMeshStruct::solution_name << "\"\n";
      apf::renameField(field, Albany::APFMeshStruct::solution_name);
    }
    restartDataTime = params->get<double>("Sim Restart Time", 0);
    solutionInitialized = true;
    apf::writeVtkFiles("restarted", mesh);
  }
}

Albany::SimMeshStruct::~SimMeshStruct()
{
  mesh->destroyNative();
  apf::destroyMesh(mesh);
  gmi_destroy(model);
  PCU_Comm_Free();
  gmi_sim_stop();
  SimPartitionedMesh_stop();
  MS_exit();
  Sim_unregisterAllKeys();
  SimUtil_stop();
}

Albany::AbstractMeshStruct::msType
Albany::SimMeshStruct::meshSpecsType()
{
  return SIM_MS;
}

apf::Field*
Albany::SimMeshStruct::createNodalField(char const* name, int valueType)
{
  return apf::createSIMFieldOn(this->mesh, name, valueType);
}

Teuchos::RCP<const Teuchos::ParameterList>
Albany::SimMeshStruct::getValidDiscretizationParameters() const
{

  Teuchos::RCP<Teuchos::ParameterList> validPL
     = APFMeshStruct::getValidDiscretizationParameters();

  validPL->set<int>("Sim Write Interval", 3, "Step interval to write solution data to output file");

  validPL->set<std::string>("Sim Input File Name", "", "File Name For Sim Mesh Input");
  validPL->set<std::string>("Sim Output File Name", "", "File Name For Sim Mesh Output");
  validPL->set<std::string>("Sim Model Input File Name", "", "File Name For Sim Mesh Output");
  validPL->set<std::string>("Sim Restart File Name", "", "read initial solution field from this file");
  validPL->set<double>("Sim Restart Time", 0, "simulation time to restart from");

  return validPL;
}


