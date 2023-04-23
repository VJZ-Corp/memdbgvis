#pragma once

#ifndef VISUALIZERPROCCOMM_H
#define VISUALIZERPROCCOMM_H

#include "pch.h"

#define NEW_SECTION output_filestream << "SECTION_END_BEGIN_NEW\n";

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

typedef struct
{
	jvmtiThreadInfo threadInfo;
	std::string metrics;
	std::vector<std::string> methodNames;
	std::vector<std::string> localVars;
	std::vector<std::string> staticFields;
	std::vector<std::string> heapByteData;
} VisualizerPayload;

class VisualizerProcComm
{
	PROCESS_INFORMATION m_piProcInfo{};
	STARTUPINFO m_siStartInfo{};
	WCHAR m_dllpath[MAX_PATH]{};
	WCHAR m_exepath[MAX_PATH]{};

public:
	VisualizerProcComm();
	~VisualizerProcComm() = default;
	static void displayErrorDialog(LPCWSTR message, HWND hWnd = nullptr);
	void launch();
	void serializeDataStruct(const VisualizerPayload& data);
};

#endif // VISUALIZERPROCCOMM_H