/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See Doc/copyright/copyright.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkSlicerColorLogic.cxx,v $
  Date:      $Date: 2006/01/06 17:56:48 $
  Version:   $Revision: 1.58 $

=========================================================================auto=*/

#include "vtkObjectFactory.h"
#include "vtkCallbackCommand.h"
#include <vtksys/SystemTools.hxx> 

#include "vtkSlicerColorLogic.h"
#include "vtkMRMLColorNode.h"
#include "vtkMRMLColorTableNode.h"
#include "vtkMRMLFreeSurferProceduralColorNode.h"
#include "vtkMRMLdGEMRICProceduralColorNode.h"
#include "vtkMRMLProceduralColorNode.h"
#include "vtkColorTransferFunction.h"

#include "vtkMRMLColorTableStorageNode.h"

#include "vtkSlicerConfigure.h" // for Slicer3_INSTALL_SHARE_DIR

#ifdef WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#endif

vtkCxxRevisionMacro(vtkSlicerColorLogic, "$Revision: 1.9.12.1 $");
vtkStandardNewMacro(vtkSlicerColorLogic);

//----------------------------------------------------------------------------
vtkSlicerColorLogic::vtkSlicerColorLogic()
{
//  this->DebugOn();
  this->UserColorFilePaths = NULL;
  
  // find the color files
  this->FindColorFiles();
}

//----------------------------------------------------------------------------
vtkSlicerColorLogic::~vtkSlicerColorLogic()
{
  // remove the default color nodes
  this->RemoveDefaultColorNodes();

  // clear out the list of files
  this->ColorFiles.clear();

  if (this->UserColorFilePaths)
    {
    delete [] this->UserColorFilePaths;
    this->UserColorFilePaths = NULL;
    }
}

//----------------------------------------------------------------------------
void vtkSlicerColorLogic::ProcessMRMLEvents(vtkObject * caller,
                                            unsigned long event,
                                            void * callData)
{
  vtkDebugMacro("vtkSlicerColorLogic::ProcessMRMLEvents: got an event " << event);
  
  // when there's a new scene, add the default nodes
  //if (event == vtkMRMLScene::NewSceneEvent || event == vtkMRMLScene::SceneCloseEvent)
  if (event == vtkMRMLScene::NewSceneEvent)
    {
    vtkDebugMacro("vtkSlicerColorLogic::ProcessMRMLEvents: got a NewScene event " << event);
    this->AddDefaultColorNodes();
    }
}

//----------------------------------------------------------------------------
void vtkSlicerColorLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);

  os << indent << "vtkSlicerColorLogic:             " << this->GetClassName() << "\n";

  os << indent << "UserColorFilePaths: " << this->GetUserColorFilePaths() << "\n";
  os << indent << "Color Files:\n";
  for (int i = 0; i << this->ColorFiles.size(); i++)
    {
    os << indent.GetNextIndent() << i << " " << this->ColorFiles[i].c_str() << "\n";
    }
}

//----------------------------------------------------------------------------
void vtkSlicerColorLogic::AddDefaultColorNodes()
{
    // create the default color nodes, they don't get saved with the scenes as
    // they'll be created on start up, and when a new
    // scene is opened
  if (this->GetMRMLScene() == NULL)
    {
    vtkWarningMacro("vtkSlicerColorLogic::AddDefaultColorNodes: no scene to which to add nodes\n");
    return;
    }
  vtkMRMLColorTableNode *basicNode = vtkMRMLColorTableNode::New();
  for (int i = basicNode->GetFirstType(); i <= basicNode->GetLastType(); i++)
    {
    // don't add a File node or the old atlas node
    if (i != basicNode->File && i != 11)
      {
      vtkMRMLColorTableNode *node = vtkMRMLColorTableNode::New();
      node->SetType(i);
      if (strcmp(node->GetTypeAsString(), "(unknown)") != 0)
        {
        node->SaveWithSceneOff();
        node->SetName(node->GetTypeAsString());      
        std::string id = std::string(this->GetDefaultColorTableNodeID(i));
        node->SetSingletonTag(id.c_str());
        if (this->GetMRMLScene()->GetNodeByID(id.c_str()) == NULL)
          {
          vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: requesting id " << id.c_str() << endl);
          this->GetMRMLScene()->RequestNodeID(node, id.c_str());
          this->GetMRMLScene()->AddNode(node);
          vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: added node " << node->GetID() << ", requested id was " << id.c_str() << ", type = " << node->GetTypeAsString() << endl);
          }
        else
          {
          vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: didn't add node " << node->GetID() << " as it was already in the scene.\n");
          }
        }
      node->Delete();
      }
    }
  basicNode->Delete();

  // add a random procedural node that covers full integer range
  vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: making a random int mrml proc color node");
  vtkMRMLProceduralColorNode *procNode = vtkMRMLProceduralColorNode::New();
  procNode->SetName("RandomIntegers");
  procNode->SaveWithSceneOff();
  const char* procNodeID = this->GetDefaultProceduralColorNodeID(procNode->GetName());
  procNode->SetSingletonTag(procNodeID);
  if (this->GetMRMLScene()->GetNodeByID(procNodeID) == NULL)
    {
    this->GetMRMLScene()->RequestNodeID(procNode, procNodeID);        
    this->GetMRMLScene()->AddNode(procNode);
    
    vtkColorTransferFunction *func = procNode->GetColorTransferFunction();
    int minValue =  VTK_INT_MIN;
    int maxValue =  VTK_INT_MAX;
    double m = (double)minValue;
    double diff = (double)maxValue - (double)minValue;
    double spacing = diff/1000.0;
    while (m <= maxValue)
      {
      double r = (rand()%255)/255.0;
      double b = (rand()%255)/255.0;
      double g = (rand()%255)/255.0;
      func->AddRGBPoint( m, r, g, b);     
      m += (int)(spacing);
      }
    func->Build();
    }

  delete [] procNodeID;
  procNode->Delete();
  
  // add freesurfer nodes
  vtkDebugMacro("AddDefaultColorNodes: Adding Freesurfer nodes");
  vtkMRMLFreeSurferProceduralColorNode *basicFSNode = vtkMRMLFreeSurferProceduralColorNode::New();
  vtkDebugMacro("AddDefaultColorNodes: first type = " <<  basicFSNode->GetFirstType() << ", last type = " <<  basicFSNode->GetLastType());
  for (int i = basicFSNode->GetFirstType(); i <= basicFSNode->GetLastType(); i++)
    {
    vtkMRMLFreeSurferProceduralColorNode *node = vtkMRMLFreeSurferProceduralColorNode::New();
    vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: setting free surfer proc color node type to " << i);
    node->SetType(i);
    node->SaveWithSceneOff();
    if (node->GetTypeAsString() == NULL)
      {
      vtkWarningMacro("Node type as string is null");      
      node->SetName("NoName");
      }
    else
      {
      vtkDebugMacro("Got node type as string " << node->GetTypeAsString());
      node->SetName(node->GetTypeAsString());
      vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: set proc node name to " << node->GetName());
      }
    /*
    if (this->GetDefaultFreeSurferColorNodeID(i) == NULL)
      {
      vtkDebugMacro("Error getting default node id for freesurfer node " << i);
      }
    */
    vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: Getting default fs color node id for type " << i);
    const char* id = this->GetDefaultFreeSurferColorNodeID(i);
    vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: got default node id " << id << " for type " << i);
    node->SetSingletonTag(id);
    vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: requesting id " << id << endl);
    if (this->GetMRMLScene()->GetNodeByID(id) == NULL)
      {
      this->GetMRMLScene()->RequestNodeID(node, id);        
      this->GetMRMLScene()->AddNode(node);
      vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: added node " << node->GetID() << ", requested id was " << id << ", type = " << node->GetTypeAsString() << endl);
      }
    else
      {
      vtkDebugMacro("vtkSlicerColorLogic::AddDefaultColorNodes: didn't add node " << node->GetID() << " as it was already in the scene.\n");
      }
    node->Delete();
    }
  
  // add a regular colour tables holding the freesurfer volume file colours and
  // surface colours
  vtkMRMLColorTableNode *node = vtkMRMLColorTableNode::New();
  node->SetTypeToFile();
  node->SaveWithSceneOff();
  node->SetScene(this->GetMRMLScene());
  // make a storage node
  vtkMRMLColorTableStorageNode *colorStorageNode1 = vtkMRMLColorTableStorageNode::New();
  colorStorageNode1->SaveWithSceneOff();
  if (this->GetMRMLScene())
    {
    this->GetMRMLScene()->AddNode(colorStorageNode1);
    node->SetAndObserveStorageNodeID(colorStorageNode1->GetID());
    }
  colorStorageNode1->Delete();
  
  vtkDebugMacro("Adding FreeSurfer Labels file node");
  std::string colorFileName;
  std::string id;
  // volume labels
  node->SetName("FreeSurferLabels");
  if (basicFSNode->GetLabelsFileName() == NULL)
    {
    vtkErrorMacro("Unable to get the labels file name, not adding");
    }
  else
    {
    colorFileName = std::string(basicFSNode->GetLabelsFileName());
    vtkDebugMacro("Trying to read colour file " << colorFileName.c_str());
  
    node->GetStorageNode()->SetFileName(colorFileName.c_str());
    if (node->GetStorageNode()->ReadData(node))
      {
      id = std::string(this->GetDefaultFreeSurferLabelMapColorNodeID());
      node->SetSingletonTag(id.c_str());
      if (this->GetMRMLScene()->GetNodeByID(id) == NULL)
        {
        this->GetMRMLScene()->RequestNodeID(node, id.c_str());        
        this->GetMRMLScene()->AddNode(node);
        }
      else
        {
        vtkDebugMacro("Unable to add a new colour node " << id.c_str() << " with freesurfer colours, from file: " << node->GetStorageNode()->GetFileName() << " as there is already a node with this id in the scene");
        }
      }
    else
      {
      vtkErrorMacro("Unable to read freesurfer colour file " << node->GetFileName());
      }
    }
  // node->Delete();

 
  basicFSNode->Delete();
  node->Delete();

  // add the dGEMRIC nodes
  vtkDebugMacro("AddDefaultColorNodes: adding dGEMRIC nodes");
  vtkMRMLdGEMRICProceduralColorNode *basicdGEMRICNode = vtkMRMLdGEMRICProceduralColorNode::New();
  for (int i = basicdGEMRICNode->GetFirstType(); i <= basicdGEMRICNode->GetLastType(); i++)
    {
    vtkMRMLdGEMRICProceduralColorNode *node = vtkMRMLdGEMRICProceduralColorNode::New();
    node->SetType(i);
    node->SaveWithSceneOff();
    if (node->GetTypeAsString() == NULL)
      {
      vtkWarningMacro("Node type as string is null");      
      node->SetName("NoName");
      }
    else
      {
      vtkDebugMacro("Got node type as string " << node->GetTypeAsString());
      node->SetName(node->GetTypeAsString());
      }
    const char *id = this->GetDefaultdGEMRICColorNodeID(i);
    node->SetSingletonTag(id);
    if (this->GetMRMLScene()->GetNodeByID(id) == NULL)
      {
      this->GetMRMLScene()->RequestNodeID(node, id);        
      this->GetMRMLScene()->AddNode(node);
      }
    node->Delete();
    }
  basicdGEMRICNode->Delete();
  
  //  file based labels
  // first check for any new ones
  this->FindColorFiles();
  for (unsigned int i = 0; i < this->ColorFiles.size(); i++)
    {
    vtkMRMLColorTableNode * node =  vtkMRMLColorTableNode::New();
    node->SetTypeToFile();
    node->SaveWithSceneOff();
    node->SetScene(this->GetMRMLScene());
    // make a storage node
    vtkMRMLColorTableStorageNode *colorStorageNode2 = vtkMRMLColorTableStorageNode::New();
    colorStorageNode2->SaveWithSceneOff();
    if (this->GetMRMLScene())
      {
      this->GetMRMLScene()->AddNode(colorStorageNode2);
      node->SetAndObserveStorageNodeID(colorStorageNode2->GetID());
      }
    colorStorageNode2->Delete();
    node->GetStorageNode()->SetFileName(this->ColorFiles[i].c_str());
    node->SetName(vtksys::SystemTools::GetFilenameName(node->GetStorageNode()->GetFileName()).c_str());
    if (node->GetStorageNode()->ReadData(node))
      {
      const char* colorNodeID = this->GetDefaultFileColorNodeID(this->ColorFiles[i].c_str());
      id =  std::string(colorNodeID);

      node->SetSingletonTag(id.c_str());
      if (this->GetMRMLScene()->GetNodeByID(id) == NULL)
        {
        this->GetMRMLScene()->RequestNodeID(node, id.c_str());
        this->GetMRMLScene()->AddNode(node);
        vtkDebugMacro("Read and added file node: " <<  this->ColorFiles[i].c_str());
        }

      delete [] colorNodeID;
      }
    else
      {
      vtkWarningMacro("Unable to read color file " << this->ColorFiles[i].c_str());
      }
    node->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkSlicerColorLogic::RemoveDefaultColorNodes()
{
  // try to find any of the default colour nodes that are still in the scene
  if (this->GetMRMLScene() == NULL)
    {
    // nothing can do, it's gone
    return;
    }
  
  vtkMRMLColorTableNode *basicNode = vtkMRMLColorTableNode::New();
  vtkMRMLColorTableNode *node;
  for (int i = basicNode->GetFirstType(); i <= basicNode->GetLastType(); i++)
    {
    // don't have a File node
    if (i != basicNode->File)
      {
      basicNode->SetType(i);
      //std::string id = std::string(this->GetDefaultColorTableNodeID(i));
      const char* id = this->GetDefaultColorTableNodeID(i);
      vtkDebugMacro("vtkSlicerColorLogic::RemoveDefaultColorNodes: trying to find node with id " << id << endl);
      node = vtkMRMLColorTableNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(id));
      if (node != NULL)
        {
        this->GetMRMLScene()->RemoveNode(node);
        }
      }
    }

  // remove freesurfer nodes
  vtkMRMLFreeSurferProceduralColorNode *basicFSNode = vtkMRMLFreeSurferProceduralColorNode::New();
  vtkMRMLFreeSurferProceduralColorNode *fsnode;
  for (int i = basicFSNode->GetFirstType(); i <= basicFSNode->GetLastType(); i++)
    {
    basicFSNode->SetType(i);
    const char* id = this->GetDefaultFreeSurferColorNodeID(i);
    vtkDebugMacro("vtkSlicerColorLogic::RemoveDefaultColorNodes: trying to find node with id " << id << endl);
    fsnode =  vtkMRMLFreeSurferProceduralColorNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(id));
    if (fsnode != NULL)
      {
      this->GetMRMLScene()->RemoveNode(fsnode);
      }
    }
  basicFSNode->Delete();

   // remove the random procedural color nodes (after the fs proc nodes as
   // getting them by class)
  int numProcNodes = this->GetMRMLScene()->GetNumberOfNodesByClass("vtkMRMLProceduralColorNode");
  for (int i = 0; i < numProcNodes; i++)
    {
    vtkMRMLProceduralColorNode *procNode =  vtkMRMLProceduralColorNode::SafeDownCast(this->GetMRMLScene()->GetNthNodeByClass(i, "vtkMRMLProceduralColorNode"));
    if (procNode != NULL &&
        strcmp(procNode->GetID(), this->GetDefaultProceduralColorNodeID(procNode->GetName())) == 0)
      {
      // it's one we added
      this->GetMRMLScene()->RemoveNode(procNode);
      }
    }
  
  // remove the fs lookup table node
  node = vtkMRMLColorTableNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(this->GetDefaultFreeSurferLabelMapColorNodeID()));
  if (node != NULL)
    {
    this->GetMRMLScene()->RemoveNode(node);
    }
  
  // remove the dGEMRIC nodes
  vtkMRMLdGEMRICProceduralColorNode *basicdGEMRICNode = vtkMRMLdGEMRICProceduralColorNode::New();
  vtkMRMLdGEMRICProceduralColorNode *dGEMRICnode;
  for (int i = basicdGEMRICNode->GetFirstType(); i <= basicdGEMRICNode->GetLastType(); i++)
    {
    basicdGEMRICNode->SetType(i);
    const char* id = this->GetDefaultdGEMRICColorNodeID(i);
    vtkDebugMacro("vtkSlicerColorLogic::RemoveDefaultColorNodes: trying to find node with id " << id << endl);
    dGEMRICnode =  vtkMRMLdGEMRICProceduralColorNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(id));
    if (dGEMRICnode != NULL)
      {
      this->GetMRMLScene()->RemoveNode(dGEMRICnode);
      }
    }
  basicdGEMRICNode->Delete();

  // remove the file based labels node
  for (unsigned int i = 0; i < this->ColorFiles.size(); i++)
    {
    node =  vtkMRMLColorTableNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(this->GetDefaultFileColorNodeID(this->ColorFiles[i].c_str())));
    if (node != NULL)
      {
      this->GetMRMLScene()->RemoveNode(node);
      }
    }
}

//----------------------------------------------------------------------------
const char *vtkSlicerColorLogic::GetDefaultColorTableNodeID(int type)
{
  const char *id;
  vtkMRMLColorTableNode *basicNode = vtkMRMLColorTableNode::New();
  basicNode->SetType(type);

  //std::string id = std::string(basicNode->GetClassName()) + std::string(basicNode->GetTypeAsString());
  id = basicNode->GetTypeAsIDString();
  basicNode->Delete();
  //basicNode = NULL;
  //return id.c_str();

  return (id);
}

//----------------------------------------------------------------------------
const char * vtkSlicerColorLogic::GetDefaultFreeSurferColorNodeID(int type)
{
  const char *id;
  vtkMRMLFreeSurferProceduralColorNode *basicNode = vtkMRMLFreeSurferProceduralColorNode::New();
  basicNode->SetType(type);

  id = basicNode->GetTypeAsIDString();
  basicNode->Delete();

  return (id);
}

//----------------------------------------------------------------------------
const char * vtkSlicerColorLogic::GetDefaultdGEMRICColorNodeID(int type)
{
  const char *id;
  vtkMRMLdGEMRICProceduralColorNode *basicNode = vtkMRMLdGEMRICProceduralColorNode::New();
  basicNode->SetType(type);
  id = basicNode->GetTypeAsIDString();
  basicNode->Delete();

  return (id);
}

//----------------------------------------------------------------------------
const char *vtkSlicerColorLogic::GetDefaultVolumeColorNodeID()
{
  return this->GetDefaultColorTableNodeID(vtkMRMLColorTableNode::Grey);
}

//----------------------------------------------------------------------------
const char *vtkSlicerColorLogic::GetDefaultLabelMapColorNodeID()
{
  return this->GetDefaultColorTableNodeID(vtkMRMLColorTableNode::Labels);
}

//----------------------------------------------------------------------------
const char *vtkSlicerColorLogic::GetDefaultModelColorNodeID()
{
  return this->GetDefaultFreeSurferColorNodeID(vtkMRMLFreeSurferProceduralColorNode::Heat);
}

//----------------------------------------------------------------------------
const char * vtkSlicerColorLogic::GetDefaultFreeSurferLabelMapColorNodeID()
{
  return this->GetDefaultFreeSurferColorNodeID(vtkMRMLFreeSurferProceduralColorNode::Labels);
}

//----------------------------------------------------------------------------
char * vtkSlicerColorLogic::GetDefaultProceduralColorNodeID(const char *name)
{
  char *id;
  std::string idStr = std::string("vtkMRMLProceduralColorNode") + std::string(name);
  id = new char[idStr.length() + 1];
  strcpy(id, idStr.c_str());
  return id;
}

//----------------------------------------------------------------------------
char *vtkSlicerColorLogic::GetDefaultFileColorNodeID(const char * fileName)
{
  char *id;
  vtksys_stl::string name = vtksys::SystemTools::GetFilenameName(fileName);
  std::string idStr = std::string("vtkMRMLColorTableNodeFile") + name;
  id =  new char[idStr.length() + 1];
  strcpy(id, idStr.c_str());
  return id;
}

//----------------------------------------------------------------------------
void vtkSlicerColorLogic::FindColorFiles()
{
    // get the slicer home dir
  vtksys_stl::string slicerHome;
  if (vtksys::SystemTools::GetEnv("Slicer3_HOME") == NULL)
    {
    if (vtksys::SystemTools::GetEnv("PWD") != NULL)
      {
      slicerHome =  vtksys_stl::string(vtksys::SystemTools::GetEnv("PWD"));
      }
    else
      {
      slicerHome =  vtksys_stl::string("");
      }
    }
  else
    {
    slicerHome = vtksys_stl::string(vtksys::SystemTools::GetEnv("Slicer3_HOME"));
    }
  // build up the vector
  vtksys_stl::vector<vtksys_stl::string> filesVector;
  filesVector.push_back(""); // for relative path
  filesVector.push_back(slicerHome);
  filesVector.push_back(vtksys_stl::string(Slicer3_INSTALL_SHARE_DIR) + "/SlicerBaseLogic/Resources/ColorFiles");
  vtksys_stl::string resourcesDirString = vtksys::SystemTools::JoinPath(filesVector);

  // now make up a vector to iterate through of dirs to look in
  std::vector<std::string> DirectoriesToCheck;

  DirectoriesToCheck.push_back(resourcesDirString);

  // add the list of dirs set from the application
  if (this->UserColorFilePaths != NULL)
    {
    vtkDebugMacro("\nFindColorFiles: got user color file paths = " << this->UserColorFilePaths);
    // parse out the list, breaking at delimiter strings
#ifdef WIN32
    const char *delim = ";";
#else
    const char *delim = ":";
#endif
    char *ptr = strtok(this->UserColorFilePaths, delim);
    while (ptr != NULL)
      {
      std::string dir = std::string(ptr);
      vtkDebugMacro("\nFindColorFiles: Adding user dir " << dir.c_str() << " to the directories to check");
      DirectoriesToCheck.push_back(dir);
      ptr = strtok(NULL, delim);     
      }
    } else { vtkDebugMacro("\nFindColorFiles: oops, the user color file paths aren't set!"); }
  
  
  // get the list of colour files in these dir
  for (unsigned int d = 0; d < DirectoriesToCheck.size(); d++)
    {
    vtksys_stl::string dirString = DirectoriesToCheck[d];
    vtkDebugMacro("\nFindColorFiles: checking for colour files in dir " << d << " = " << dirString.c_str());

    filesVector.clear();
    filesVector.push_back(dirString);
    filesVector.push_back(vtksys_stl::string("/"));

#ifdef WIN32
    WIN32_FIND_DATA findData;
    HANDLE fileHandle;
    int flag = 1;
    std::string search ("*.*");
    dirString += "/";
    search = dirString + search;
    
    fileHandle = FindFirstFile(search.c_str(), &findData);
    if (fileHandle != INVALID_HANDLE_VALUE)
      {
      while (flag)
        {
        // add this file to the vector holding the base dir name so check the
        // file type using the full path
        filesVector.push_back(vtksys_stl::string(findData.cFileName));
#else
    DIR *dp;
    struct dirent *dirp;
    if ((dp  = opendir(dirString.c_str())) == NULL)
      {
      vtkErrorMacro("Error(" << errno << ") opening " << dirString.c_str());
      }
    else
      {
      while ((dirp = readdir(dp)) != NULL)
        {
        // add this file to the vector holding the base dir name
        filesVector.push_back(vtksys_stl::string(dirp->d_name));
#endif
        
        vtksys_stl::string fileToCheck = vtksys::SystemTools::JoinPath(filesVector); 
        int fileType = vtksys::SystemTools::DetectFileType(fileToCheck.c_str());
        if (fileType == vtksys::SystemTools::FileTypeText)
          {
          vtkDebugMacro("\nAdding " << fileToCheck.c_str() << " to list of potential colour files. Type = " << fileType);
          this->AddColorFile(fileToCheck.c_str());
          }
        else
          {
          vtkDebugMacro("\nSkipping potential colour file " << fileToCheck.c_str() << ", file type = " << fileType);
          }
        // take this file off so that can build the next file name
        filesVector.pop_back();

#ifdef WIN32
        flag = FindNextFile(fileHandle, &findData);
        } // end of while flag
      FindClose(fileHandle);
      } // end of having a valid fileHandle
#else
        } // end of while loop over reading the directory entries
      closedir(dp);
      } // end of able to open dir
#endif

    } // end of looping over dirs
}

//----------------------------------------------------------------------------
void vtkSlicerColorLogic::AddColorFile(const char *fileName)
{
  if (fileName == NULL)
    {
    vtkErrorMacro("AddColorFile: can't add a null color file name");
    return;
    }
  // check if it's in the vector already
  std::string fileNameStr = std::string(fileName);
  for (unsigned int i = 0; i <  this->ColorFiles.size(); i++)
    {
    if (this->ColorFiles[i] == fileNameStr)
      {
      vtkDebugMacro("\nAddColorFile: already have this file at index " << i << ", not adding it again: " << fileNameStr.c_str());
      return;
      }
    }
  vtkDebugMacro("\nAddColorFile: adding file name to ColorFiles: " << fileNameStr.c_str());
  this->ColorFiles.push_back(fileNameStr);
}

//----------------------------------------------------------------------------
vtkMRMLColorNode * vtkSlicerColorLogic::LoadColorFile(const char *fileName)
{
  vtkMRMLColorTableNode *node = vtkMRMLColorTableNode::New();
  
  node->SetTypeToFile();
  node->SetScene(this->GetMRMLScene());
  // make a storage node
  vtkMRMLColorTableStorageNode *colorStorageNode = vtkMRMLColorTableStorageNode::New();
  colorStorageNode->SaveWithSceneOff();
  if (this->GetMRMLScene())
    {
    this->GetMRMLScene()->AddNode(colorStorageNode);
    node->SetAndObserveStorageNodeID(colorStorageNode->GetID());
    }
  
  node->SetFileName(fileName);
  colorStorageNode->SetFileName(fileName);
  
  node->SetName(vtksys::SystemTools::GetFilenameName(node->GetFileName()).c_str());
  std::string id;
  if (colorStorageNode->ReadData(node)) // ReadFile())
    {
    id =  std::string(this->GetDefaultFileColorNodeID(colorStorageNode->GetFileName()));
    if (this->GetMRMLScene()->GetNodeByID(id) == NULL)
      {
      this->GetMRMLScene()->RequestNodeID(node, id.c_str());
      this->GetMRMLScene()->AddNode(node);
      vtkDebugMacro("Read and added file node: " <<  fileName);
      this->AddColorFile(fileName);
      colorStorageNode->Delete();
      return node;
      }
    }
  else
    {
    vtkWarningMacro("Unable to read color file " << fileName);
    }
  colorStorageNode->Delete();
  return NULL;
}

