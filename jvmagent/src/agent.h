#pragma once

#ifndef AGENT_H
#define AGENT_H

#include "pch.h"
#include "visualizerproccomm.h"

namespace Agent
{
	const std::unordered_map<char, std::string> mappings = {
		{'Z', "boolean"}, {'B', "byte"}, {'C', "char"},
		{'S', "short"}, {'I', "int"}, {'J', "long"},
		{'F', "float"}, {'D', "double"}, {'V', "void"}
	};

	static bool catchJVMTIError(jvmtiEnv* jvmti, jvmtiError error, const std::string& errmsg, bool silent = false);
    static void JNICALL callbackEventHandler(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jmethodID method, jlocation location, jobject exception);
	static std::string dataTypeFormatter(std::string unformatted);
	static std::string decodeJVMTypeSignature(const std::string& name, const std::string& signature, bool isMethod = false);
}

#endif // AGENT_H