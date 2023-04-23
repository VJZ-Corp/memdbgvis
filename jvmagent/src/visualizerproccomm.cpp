#include "pch.h"
#include "visualizerproccomm.h"

VisualizerProcComm::VisualizerProcComm()
{
	// get the agent path in order to find the visualizer executable
	if (!GetModuleFileName(reinterpret_cast<HINSTANCE>(&__ImageBase), this->m_dllpath, _countof(this->m_dllpath)))
		VisualizerProcComm::displayErrorDialog((L"Cannot locate the agent path: " + std::wstring(this->m_dllpath)).c_str());

	// change the file extension to .exe instead of .dll
	for (size_t i = 0; i < MAX_PATH; i++)
	{
		this->m_exepath[i] = this->m_dllpath[i];

		// if array is not filled up completely
		if (i > 2 && this->m_exepath[i] == '\0')
		{
			this->m_exepath[i - 1] = 'e';
			this->m_exepath[i - 2] = 'x';
			this->m_exepath[i - 3] = 'e';
			break;
		}
	}
}

void VisualizerProcComm::displayErrorDialog(const LPCWSTR message, HWND hWnd)
{
	if (!MessageBox(hWnd, message, L"Memory Debug Visualizer", MB_ICONERROR | MB_OK))
		throw std::exception("Message box dialog cannot be initialized.");
}

void VisualizerProcComm::launch()
{
	ZeroMemory(&this->m_piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&this->m_siStartInfo, sizeof(STARTUPINFO));
	this->m_siStartInfo.cb = sizeof(STARTUPINFO);

	// launch visualizer executable
	const BOOL success = CreateProcess(
		this->m_exepath, 
		nullptr, 
		nullptr, 
		nullptr, 
		FALSE, 
		0, 
		nullptr, 
		nullptr, 
		&this->m_siStartInfo,
		&this->m_piProcInfo
	);

	// may fail if the user places the visualizer in a path longer than MAX_PATH
	if (!success)
		VisualizerProcComm::displayErrorDialog((L"Cannot start process: " + std::wstring(this->m_exepath)).c_str());

	// pause current thread until user closes visualizer window
	do Sleep(999);
	while (WaitForSingleObject(this->m_piProcInfo.hProcess, 0) == WAIT_TIMEOUT);

	// resource cleanup
	CloseHandle(this->m_piProcInfo.hProcess);
	CloseHandle(this->m_piProcInfo.hThread);
}

void VisualizerProcComm::serializeDataStruct(const VisualizerPayload& data)
{
	// setup file object for writing
	auto filepath = std::wstring(this->m_exepath);
	filepath.erase(filepath.size() - 3); // remove 'exe' extension
	filepath += L"dat"; // replace extension with .dat

	/*
	 * The line number of invocation is not easily accessible by using JVMTI.
	 * Therefore, the java exception wrapper class that is responsible
	 * for invoking this agent writes the line number to the data file first.
	 * Before this agent overwrites that information, the agent needs to store
	 * the line number in its working memory then rewrite it back to the data file.
	 * This redundant setup ensures the files are synchronized for access.
	 */
	std::ifstream input_filestream(filepath);
	std::string line_num;
	input_filestream >> line_num;
	input_filestream.close();

	// overwrite previous contents when opening new file stream
	std::ofstream output_filestream(filepath);
	output_filestream << line_num << '\n';
	output_filestream << "NAME: " << data.threadInfo.name << '\n';
	
	switch (data.threadInfo.priority)
	{
	case JVMTI_THREAD_MIN_PRIORITY:
		output_filestream << "PRIORITY: MINIMUM\n";
		break;
	case JVMTI_THREAD_NORM_PRIORITY:
		output_filestream << "PRIORITY: NORMAL\n";
		break;
	case JVMTI_THREAD_MAX_PRIORITY:
		output_filestream << "PRIORITY: MAXIMUM\n";
		break;
	default:
		output_filestream << "PRIORITY: UNKNOWN\n";
	}

	// serialize runtime metrics
	output_filestream << data.metrics << '\n';

	// serialize call stack view
	NEW_SECTION
	for (const std::string& method_name : data.methodNames)
		output_filestream << method_name << '\n';

	// serialize local variable table
	NEW_SECTION
	for (const std::string& var : data.localVars)
		output_filestream << var << '\n';

	// serialize class field table
	NEW_SECTION
	for (const std::string& field : data.staticFields)
		output_filestream << field << '\n';

	// serialize heap data
	NEW_SECTION
	for (const std::string& bytes : data.heapByteData)
		output_filestream << bytes << '\n';
}