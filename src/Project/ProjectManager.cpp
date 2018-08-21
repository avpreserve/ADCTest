#include "ProjectManager.h"

#include <wx/app.h>
#include <wx/config.h>
#include <wx/intl.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/datetime.h>

ProjectManager::ProjectManager()
:mProjectNode(NULL)
,mTemplateNode(NULL)
,bProjLoaded(false)
,bTemplateLoaded(false)
{
	mFileSeparator = wxT('\\');
	mTemplateName = wxT("default");

	wxString exePath = wxStandardPaths::Get().GetExecutablePath();
	mExePath = exePath.BeforeLast(mFileSeparator);
}


ProjectManager::~ProjectManager()
{
}

bool ProjectManager::LoadTemplate(wxString path)
{
	bool bLoaded = false;

	if (mTemplateNode) {
		delete mTemplateNode;
		mTemplateNode = NULL;
		bLoaded = false;
	}

	if (path.IsEmpty())
		path = mExePath + mFileSeparator + mTemplateName + wxT(".avt");

	mTemplateNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("unknown"));
	mTemplateNode->AddAttribute(wxT("title"), wxT("unknown"));

	wxXmlDocument* readSchema = new wxXmlDocument();
	if (readSchema->Load(path))
	{
		mTemplateNode = readSchema->DetachRoot();
		bLoaded = true;
	}
	delete readSchema;

	return bLoaded;
}

int ProjectManager::NewProject(wxString path, wxString name, std::vector<ProjectInfo> info)
{
	bTemplateLoaded = LoadTemplate(wxEmptyString);

	if (!bTemplateLoaded)
		return MgmtErrorTemplateReadFail;

	mProjectTitle = name;
	mProjectFolder = path + mFileSeparator + name;
	mProjectPath = mProjectFolder + mFileSeparator + name + wxT(".avt");

	//create folders
	if (wxDirExists(mProjectFolder))
		return MgmtErrorFolderExists;

	if (!wxMkdir(mProjectFolder))
		return MgmtErrorFolderWriteFail;

	//set project title
	mProjectNode = mTemplateNode;
	mProjectNode->DeleteAttribute(wxT("title"));
	mProjectNode->AddAttribute(wxT("title"), mProjectTitle);
	
	//set project date	
	wxString projectDate;

	wxDateTime now = wxDateTime::Now();

	projectDate += wxString::Format("%d/", now.GetDay());
	projectDate += wxString::Format("%d/", now.GetMonth()+1);
	projectDate += wxString::Format("%d ", now.GetYear());
	projectDate += wxString::Format(" at %d:", now.GetHour());
	projectDate += wxString::Format("%d", now.GetMinute());

	mProjectNode->DeleteAttribute(wxT("date"));
	mProjectNode->AddAttribute(wxT("date"), projectDate);

	SetProjectInfo(info);

	if (!SaveProject())
		return 	MgmtErrorProjectWriteFail;

	return OpenProject(mProjectPath);
}

void ProjectManager::SetProjectInfo(std::vector<ProjectInfo> info)
{
	if (mProjectNode)
	{
		//remove previous info node;
		wxXmlNode* cNode = mProjectNode->GetChildren();
		while (cNode)
		{
			if (cNode->GetName() == wxT("projectinfo")) {
				mProjectNode->RemoveChild(cNode);
				delete cNode;
				break;
			}
			cNode = cNode->GetNext();
		}

		//put an updated nfo node;

		wxXmlNode* prjInfoNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("projectinfo"));

		ProjectInfo pi;
		for (size_t i = 0; i < info.size(); i++)
		{
			pi = info[i];
			wxXmlNode* infoEntry = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("info"));
			infoEntry->AddAttribute(wxT("name"), pi.infoName);
			infoEntry->AddAttribute(wxT("value"), pi.infoValue);
			prjInfoNode->AddChild(infoEntry);
		}

		mProjectNode->AddChild(prjInfoNode);
	}
}

std::vector<ProjectInfo> ProjectManager::GetProjectInfo()
{
	std::vector<ProjectInfo> info;

	if (mProjectNode)
	{
		//remove previous info node;
		wxXmlNode* cNode = mProjectNode->GetChildren();
		while (cNode)
		{
			if (cNode->GetName() == wxT("projectinfo")) 
			{
				ProjectInfo ie;

				wxXmlNode* infoEntry = cNode->GetChildren();
				while (infoEntry) {

					wxString entryName = infoEntry->GetAttribute(wxT("name"));
					wxString entryValue = infoEntry->GetAttribute(wxT("value"));
					ie.infoName =  entryName;
					ie.infoValue = entryValue;
					info.push_back(ie);
					infoEntry = infoEntry->GetNext();
				}
				break;
			}
			cNode = cNode->GetNext();
		}
	}
	return info;
}

void 
ProjectManager::SetProjectRemarks(wxString data)
{
	if (mProjectNode)
	{
		//remove previous remarks node;
		wxXmlNode* cNode = mProjectNode->GetChildren();
		while (cNode)
		{
			if (cNode->GetName() == wxT("remarks")) {
				mProjectNode->RemoveChild(cNode);
				delete cNode;
				break;
			}
			cNode = cNode->GetNext();
		}

		//put an updated remarks node;
		wxXmlNode* remarksNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("remarks"));
		remarksNode->AddAttribute(wxT("content"), data);
		mProjectNode->AddChild(remarksNode);
	}
}

wxString 
ProjectManager::GetProjectRemarks()
{
	wxString data;
	if (mProjectNode)
	{
		//remove previous info node;
		wxXmlNode* cNode = mProjectNode->GetChildren();
		while (cNode)
		{
			if (cNode->GetName() == wxT("remarks"))
			{
				data = cNode->GetAttribute(wxT("content"));
				break;
			}
			cNode = cNode->GetNext();
		}
	}
	return data;
}

int 
ProjectManager::OpenProject(wxString path)
{
	int result = MgmtErrorProjectReadFail;

	DeleteProject();

	mProjectNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("unknown"));
	mProjectNode->AddAttribute(wxT("title"), wxT("unknown"));

	mProjectPath = path;
	mProjectFolder = path.BeforeLast(mFileSeparator);

	wxXmlDocument* readSchema = new wxXmlDocument();
	if (readSchema->Load(path))
	{
		mProjectNode = readSchema->DetachRoot();
		
		if (ParseProject())
		{
			mProjectTitle = mProjectNode->GetAttribute("title");
			mProjectDate  = mProjectNode->GetAttribute("date");
			
			TestManager* tm = gAudioIO->GetTestManager();
			tm->SetTestXml(mProceduresNode, mProjectFolder);

			bProjLoaded = true;
			result = MgmtOK;
		}
	}
	delete readSchema;

	return result;
}

bool ProjectManager::CloseProject()
{
	DeleteProject();
	return true;
}

bool ProjectManager::SaveProject()
{
	bool bres = false;
	wxXmlDocument* writeSchema = new wxXmlDocument();
	writeSchema->SetRoot(mProjectNode);
	bres = writeSchema->Save(mProjectPath);
	writeSchema->DetachRoot();
	delete writeSchema;
	return bres;
}

void ProjectManager::DeleteProject()
{
	if (mProjectNode)
	{
		mProjectPath = wxEmptyString;
		mProjectTitle = wxEmptyString;
		mProjectDate = wxEmptyString;

		delete mProjectNode;
		mProjectNode = NULL;
		mProceduresNode = NULL;
		bProjLoaded = false;

		TestManager* tm = gAudioIO->GetTestManager();
		tm->SetTestXml(mProceduresNode, wxEmptyString);
	}
}

bool ProjectManager::ParseProject()
{
	if (mProjectNode->GetName() == wxT("FADGIProject"))
	{
		wxXmlNode* cNode = mProjectNode->GetChildren();
		while (cNode)
		{
			if (cNode->GetName() == wxT("procedures")) {
				mProceduresNode = cNode;
				break;
			}
			cNode = cNode->GetNext();
		}
		return true;
	}
	return false;
}

std::vector<TestDescriptor> 
ProjectManager::GetTestDescriptors()
{
	TestManager* tm = gAudioIO->GetTestManager();
	return tm->GetTestDescriptors();
}

std::vector<TestParameter>  
ProjectManager::GetTestParameters(wxString testID)
{
	TestManager* tm = gAudioIO->GetTestManager();
	return tm->GetTestParameters(testID);
}

void 
ProjectManager::EnableTest(wxString testID, bool enabled)
{
	TestManager* tm = gAudioIO->GetTestManager();
	tm->EnableTest(testID, enabled);
}

void 
ProjectManager::SetTestType(wxString testID, wxString testMode)
{
	TestManager* tm = gAudioIO->GetTestManager();
	tm->SetTestType(testID, testMode);
}

void 
ProjectManager::SetTestResponseFileName(wxString testID, wxString fileName)
{
	TestManager* tm = gAudioIO->GetTestManager();
	tm->SetTestResponseFileName(testID, fileName);
}

TestFileIOInfo
ProjectManager::GetTestFileIOInfo(wxString testID)
{
	TestManager* tm = gAudioIO->GetTestManager();
	return tm->GetTestFileIOInfo(testID);
}

TestAudioIOInfo 
ProjectManager::GetTestAudioIOInfo(wxString testID)
{
	TestManager* tm = gAudioIO->GetTestManager();
	return tm->GetTestAudioIOInfo(testID);
}

void 
ProjectManager::SetTestSignalChannel(wxString testID, wxString chIdx)
{
	TestManager* tm = gAudioIO->GetTestManager();
	tm->SetTestSignalChannel(testID, chIdx);
}

void 
ProjectManager::SetTestResponseChannel(wxString testID, wxString chIdx)
{
	TestManager* tm = gAudioIO->GetTestManager();
	tm->SetTestResponseChannel(testID, chIdx);
}
