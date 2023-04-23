#include "pch.h"
#include "agent.h"

// special function that serves as the entrypoint for the JVM DLL agent
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved)
{
	// fetch JVMTI environment from JVM
	jvmtiEnv* jvmti;
	const jint result = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION);
	if (result != JNI_OK)
		return result;

	// set capabilities for the agent
	jvmtiCapabilities capabilities = {};
	capabilities.can_generate_exception_events = JNI_TRUE;
	capabilities.can_access_local_variables = JNI_TRUE;
	jvmtiError error = jvmti->AddCapabilities(&capabilities);
	if (Agent::catchJVMTIError(jvmti, error, "Unable to set agent capabilities."))
		return JNI_ERR;

	// set the event notification mode
	error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION_CATCH, nullptr);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot set event notification mode."))
		return JNI_ERR;

	// assign a callback as a event handler
	jvmtiEventCallbacks callbacks = {};
	callbacks.ExceptionCatch = &Agent::callbackEventHandler;
	error = jvmti->SetEventCallbacks(&callbacks, sizeof callbacks);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot set event callbacks."))
		return JNI_ERR;

	// return OK status if everything works
	return JNI_OK;
}

// function that handles JVMTI errors
static bool Agent::catchJVMTIError(jvmtiEnv* jvmti, jvmtiError error, const std::string& errmsg, const bool silent)
{
	if (error == JVMTI_ERROR_NONE)
		return false;

	if (silent)
		return true;

	// display an error dialog
	char* errname = nullptr;
	static_cast<void>(jvmti->GetErrorName(error, &errname));
	const std::string errstr = errname;
	VisualizerProcComm::displayErrorDialog((std::wstring(errstr.begin(), errstr.end()) + L": " + std::wstring(errmsg.begin(), errmsg.end())).c_str());
	return true;
}

// core backbone callback function that handles critical operations of the agent
static void Agent::callbackEventHandler(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jmethodID method, jlocation location, jobject exception)
{
	// get class signature of exception
	char* exception_signature;
	const jclass exception_class = env->GetObjectClass(exception);
	jvmtiError error = jvmti->GetClassSignature(exception_class, &exception_signature, nullptr);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get JVM class signature."))
		return;

	// visualizer invoked only if called from memdbgvis exception class
	if (std::string(exception_signature) != "Lcom/vjzcorp/jvmtools/memdbgvis;")
		return;

	// get stack depth
	jint count;
	error = jvmti->GetFrameCount(thread, &count);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get stack frame count."))
		return;

	// initialize array of stack frames
	const auto frames = std::make_unique<jvmtiFrameInfo[]>(count);
	error = jvmti->GetStackTrace(thread, 1, count, frames.get(), &count);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get stack frames."))
		return;

	// create visualizer communication objects
	VisualizerProcComm visualizer;
	VisualizerPayload payload;
	jmethodID toStringMethod = env->GetMethodID(exception_class, "toString", "()Ljava/lang/String;");

	// get thread info and load it into payload
	error = jvmti->GetThreadInfo(thread, &payload.threadInfo);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get current thread name."))
		return;

	// get miscellaneous JVM metrics
	jmethodID getRuntimeMetricsMethod = env->GetStaticMethodID(exception_class, "getRuntimeMetrics", "()Ljava/lang/String;");
	auto jmetrics = reinterpret_cast<jstring>(env->CallObjectMethod(env->CallStaticObjectMethod(exception_class, getRuntimeMetricsMethod), toStringMethod));
	const char* cmetrics = env->GetStringUTFChars(jmetrics, nullptr);
	payload.metrics = cmetrics;
	env->ReleaseStringUTFChars(jmetrics, cmetrics);

	// populate call stack view with method names
	for (jint i = 0; i < count; i++)
	{
		char* method_name;
		char* method_signature;
		std::string decoded_signature;
		jint modifiers;

		error = jvmti->GetMethodName(frames[i].method, &method_name, &method_signature, nullptr);
		if (Agent::catchJVMTIError(jvmti, error, "Cannot get current method name."))
			continue;

		error = jvmti->GetMethodModifiers(frames[i].method, &modifiers);
		if (Agent::catchJVMTIError(jvmti, error, "Cannot get current method modifiers."))
			continue;

		// if method is static
		if (modifiers & JVMTI_HEAP_REFERENCE_STATIC_FIELD)
			decoded_signature = "static ";

		// load method names into payload struct
		decoded_signature += Agent::decodeJVMTypeSignature(std::string(method_name), std::string(method_signature), true);
		payload.methodNames.emplace_back(decoded_signature);
	}

	// get local variables in current stack frame
	jvmtiLocalVariableEntry* local_var_table;
	error = jvmti->GetLocalVariableTable(frames[0].method, &count, &local_var_table);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get local variable table for current stack frame."))
		goto serialize_launch;

	// enumerate through all local variables
	for (jint i = 0; i < count; i++)
	{
		/* char and boolean representations will be handled by the robust Qt framework in the visualizer */
		if (*local_var_table[i].signature == 'B' || *local_var_table[i].signature == 'S' || *local_var_table[i].signature == 'I' || *local_var_table[i].signature == 'C' || *local_var_table[i].signature == 'Z')
		{
			jint value;
			error = jvmti->GetLocalInt(thread, 1, local_var_table[i].slot, &value);
			if (Agent::catchJVMTIError(jvmti, error, "Cannot get local variable of integer type.", true))
				continue;
			payload.localVars.push_back(Agent::decodeJVMTypeSignature(local_var_table[i].name, local_var_table[i].signature) + '\a' + std::to_string(value));
		}
		else if (*local_var_table[i].signature == 'D') /* local variables of double type */
		{
			jdouble double_value;
			error = jvmti->GetLocalDouble(thread, 1, local_var_table[i].slot, &double_value);
			if (Agent::catchJVMTIError(jvmti, error, "Cannot get local variable of double type.", true))
				continue;
			payload.localVars.push_back(Agent::decodeJVMTypeSignature(local_var_table[i].name, local_var_table[i].signature) + '\a' + std::to_string(double_value));
		}
		else if (*local_var_table[i].signature == 'F')  /* local variables of float type */
		{
			jfloat float_value;
			error = jvmti->GetLocalFloat(thread, 1, local_var_table[i].slot, &float_value);
			if (Agent::catchJVMTIError(jvmti, error, "Cannot get local variable of float type.", true))
				continue;
			payload.localVars.push_back(Agent::decodeJVMTypeSignature(local_var_table[i].name, local_var_table[i].signature) + '\a' + std::to_string(float_value));
		}
		else if (*local_var_table[i].signature == 'J') /* local variables of long type */
		{
			jlong long_value;
			error = jvmti->GetLocalLong(thread, 1, local_var_table[i].slot, &long_value);
			if (Agent::catchJVMTIError(jvmti, error, "Cannot get local variable of long type.", true))
				continue;
			payload.localVars.push_back(Agent::decodeJVMTypeSignature(local_var_table[i].name, local_var_table[i].signature) + '\a' + std::to_string(long_value));
		}
		else if (*local_var_table[i].signature == '[' || *local_var_table[i].signature == 'L') /* local object references */
		{
			// get local object reference
			jobject obj;
			error = jvmti->GetLocalObject(thread, 1, local_var_table[i].slot, &obj);
			if (Agent::catchJVMTIError(jvmti, error, "Cannot get object reference.", true))
				continue;

			// null reference
			if (obj == nullptr)
			{
				payload.localVars.push_back(Agent::decodeJVMTypeSignature(local_var_table[i].name, local_var_table[i].signature) + '\a' + "null");
				continue;
			}

			// make it string serializable
			auto jstr = reinterpret_cast<jstring>(env->CallObjectMethod(obj, toStringMethod));
			const char* cstr = env->GetStringUTFChars(jstr, nullptr);
			std::string str(cstr);
			str = std::regex_replace(str, std::regex("\n"), "\\n");
			str = std::regex_replace(str, std::regex("\r"), "\\r");
			env->ReleaseStringUTFChars(jstr, cstr);
			payload.localVars.push_back(Agent::decodeJVMTypeSignature(local_var_table[i].name, local_var_table[i].signature) + '\a' + str);

			// get contents of array 
			if (*local_var_table[i].signature == '[')
			{
				std::stringstream stream;
				stream << "{ ";

				if (std::string(local_var_table[i].signature).back() == 'I') /* int[] */
				{
					auto array = reinterpret_cast<jintArray>(obj);
					jint* elements = env->GetIntArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						stream << elements[j] << ", ";

					stream << elements[env->GetArrayLength(array) - 1] << " }";
					env->ReleaseIntArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'B') /* byte[] */
				{
					auto array = reinterpret_cast<jbyteArray>(obj);
					jbyte* elements = env->GetByteArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						stream << int{ elements[j] } << ", ";

					stream << int{ elements[env->GetArrayLength(array) - 1] } << " }";
					env->ReleaseByteArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'S') /* short[] */
				{
					auto array = reinterpret_cast<jshortArray>(obj);
					jshort* elements = env->GetShortArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						stream << elements[j] << ", ";

					stream << elements[env->GetArrayLength(array) - 1] << " }";
					env->ReleaseShortArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'J') /* long[] */
				{
					auto array = reinterpret_cast<jlongArray>(obj);
					jlong* elements = env->GetLongArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						stream << elements[j] << ", ";

					stream << elements[env->GetArrayLength(array) - 1] << " }";
					env->ReleaseLongArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'F') /* float[] */
				{
					auto array = reinterpret_cast<jfloatArray>(obj);
					jfloat* elements = env->GetFloatArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						stream << elements[j] << ", ";

					stream << elements[env->GetArrayLength(array) - 1] << " }";
					env->ReleaseFloatArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'D') /* double[] */
				{
					auto array = reinterpret_cast<jdoubleArray>(obj);
					jdouble* elements = env->GetDoubleArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						stream << elements[j] << ", ";

					stream << elements[env->GetArrayLength(array) - 1] << " }";
					env->ReleaseDoubleArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'C') /* char[] */
				{
					auto array = reinterpret_cast<jcharArray>(obj);
					jchar* elements = env->GetCharArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
					{
						if (elements[j] == '\n')
							stream << R"('\n', )";
						else if (elements[j] == '\r')
							stream << R"('\r', )";
						else
							stream << '\'' << static_cast<char>(elements[j]) << "', ";
					}

					if (elements[env->GetArrayLength(array) - 1] == '\n')
						stream << R"('\n' })";
					else if (elements[env->GetArrayLength(array) - 1] == '\r')
						stream << R"('\r' })";
					else
						stream << '\'' << static_cast<char>(elements[env->GetArrayLength(array) - 1]) << "' }";

					env->ReleaseCharArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).back() == 'Z') /* boolean[] */
				{
					auto array = reinterpret_cast<jbooleanArray>(obj);
					jboolean* elements = env->GetBooleanArrayElements(array, nullptr);

					for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
					{
						if (elements[j])
							stream << "true, ";
						else
							stream << "false, ";
					}

					if (elements[env->GetArrayLength(array) - 1])
						stream << "true }";
					else
						stream << "false }";

					env->ReleaseBooleanArrayElements(array, elements, JNI_ABORT);
				}
				else if (std::string(local_var_table[i].signature).find("[L") != std::string::npos) /* custom object array */
				{
					auto jarray = reinterpret_cast<jobjectArray>(obj);

					for (jint j = 0; j < env->GetArrayLength(jarray); j++)
					{
						jobject element = env->GetObjectArrayElement(jarray, j);
						auto element_str = reinterpret_cast<jstring>(env->CallObjectMethod(element, toStringMethod));
						const char* element_cstr;

						if (element == nullptr)
							element_cstr = "null";
						else
							element_cstr = env->GetStringUTFChars(element_str, nullptr);

						std::string estr(element_cstr);
						estr = std::regex_replace(estr, std::regex("\n"), "\\n");
						estr = std::regex_replace(estr, std::regex("\r"), "\\r");

						if (j == env->GetArrayLength(jarray) - 1)
							stream << estr << " }";
						else
							stream << estr << ", ";
					}
				}

				if (stream.str().length() <= 2) // empty array
					stream << '}';

				payload.heapByteData.push_back(str + '\a' + stream.str());
			}
			else if (*local_var_table[i].signature == 'L')
			{
				// call java method because jvmti has no suitable function for turning objects into raw bytes
				jmethodID objectToBytesMethod = env->GetStaticMethodID(exception_class, "objectToBytes", "(Ljava/lang/Object;)[B");
				jobject rawByteArray = env->CallStaticObjectMethod(exception_class, objectToBytesMethod, obj);
				std::stringstream stream;
				auto array = reinterpret_cast<jbyteArray>(rawByteArray);
				jbyte* bytes = env->GetByteArrayElements(array, nullptr);

				// if object is already in a string format, no need to generate hex dump
				if (rawByteArray == nullptr || str.find('@') == std::string::npos)
					continue;

				for (jint j = 0; j < env->GetArrayLength(array); j++)
					stream << std::hex << int{ bytes[j] } << ' ';

				payload.heapByteData.push_back(str + '\a' + stream.str());
				env->ReleaseByteArrayElements(array, bytes, JNI_ABORT);
			}
		}
	}
	
	jclass current_class;
	error = jvmti->GetMethodDeclaringClass(frames[0].method, &current_class);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get current class."))
		goto serialize_launch;

	// get all fields in the current class
	jfieldID* fields;
	error = jvmti->GetClassFields(current_class, &count, &fields);
	if (Agent::catchJVMTIError(jvmti, error, "Cannot get class fields."))
		goto serialize_launch;

	// get modifiers for the fields
	for (jint i = 0; i < count; i++)
	{
		char* name;
		char* signature;
		jint modifiers;

		error = jvmti->GetFieldModifiers(current_class, fields[i], &modifiers);
		if (Agent::catchJVMTIError(jvmti, error, "Cannot get current field modifiers."))
			continue;
		
		error = jvmti->GetFieldName(current_class, fields[i], &name, &signature, nullptr);
		if (Agent::catchJVMTIError(jvmti, error, "Cannot get current field name."))
			continue;

		if (modifiers & JVMTI_HEAP_REFERENCE_STATIC_FIELD) // static fields
		{
			if (*signature == 'I')
			{
				const jint value = env->GetStaticIntField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(value));
			}
			else if (*signature == 'B')
			{
				const jbyte value = env->GetStaticByteField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(value));
			}
			else if (*signature == 'C')
			{
				const jchar value = env->GetStaticCharField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(value));
			}
			else if (*signature == 'S')
			{
				const jshort value = env->GetStaticShortField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(value));
			}
			else if (*signature == 'Z')
			{
				const jboolean value = env->GetStaticBooleanField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(value));
			}
			else if (*signature == 'D')
			{
				const jdouble double_value = env->GetStaticDoubleField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(double_value));
			}
			else if (*signature == 'F')
			{
				const jfloat float_value = env->GetStaticFloatField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(float_value));
			}
			else if (*signature == 'J')
			{
				const jlong long_value = env->GetStaticLongField(current_class, fields[i]);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + std::to_string(long_value));
			}
			else if (*signature == '[' || *signature == 'L')
			{
				jobject obj = env->GetStaticObjectField(current_class, fields[i]);
				auto jstr = reinterpret_cast<jstring>(env->CallObjectMethod(obj, toStringMethod));

				// null reference
				if (jstr == nullptr)
				{
					payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + "null");
					continue;
				}

				const char* cstr = env->GetStringUTFChars(jstr, nullptr);
				std::string str(cstr);
				str = std::regex_replace(str, std::regex("\n"), "\\n");
				str = std::regex_replace(str, std::regex("\r"), "\\r");
				env->ReleaseStringUTFChars(jstr, cstr);
				payload.staticFields.push_back("static " + Agent::decodeJVMTypeSignature(name, signature) + '\a' + str);

				// get contents of array 
				if (*signature == '[')
				{
					std::stringstream stream;
					stream << "{ ";

					if (std::string(signature).back() == 'I') /* static int[] */
					{
						auto array = reinterpret_cast<jintArray>(obj);
						jint* elements = env->GetIntArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
							stream << elements[j] << ", ";

						stream << elements[env->GetArrayLength(array) - 1] << " }";
						env->ReleaseIntArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'B') /* static byte[] */
					{
						auto array = reinterpret_cast<jbyteArray>(obj);
						jbyte* elements = env->GetByteArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
							stream << int{ elements[j] } << ", ";

						stream << int{ elements[env->GetArrayLength(array) - 1] } << " }";
						env->ReleaseByteArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'S') /* static short[] */
					{
						auto array = reinterpret_cast<jshortArray>(obj);
						jshort* elements = env->GetShortArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
							stream << elements[j] << ", ";

						stream << elements[env->GetArrayLength(array) - 1] << " }";
						env->ReleaseShortArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'J') /* static long[] */
					{
						auto array = reinterpret_cast<jlongArray>(obj);
						jlong* elements = env->GetLongArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
							stream << elements[j] << ", ";

						stream << elements[env->GetArrayLength(array) - 1] << " }";
						env->ReleaseLongArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'F') /* static float[] */
					{
						auto array = reinterpret_cast<jfloatArray>(obj);
						jfloat* elements = env->GetFloatArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
							stream << elements[j] << ", ";

						stream << elements[env->GetArrayLength(array) - 1] << " }";
						env->ReleaseFloatArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'D') /* static double[] */
					{
						auto array = reinterpret_cast<jdoubleArray>(obj);
						jdouble* elements = env->GetDoubleArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
							stream << elements[j] << ", ";

						stream << elements[env->GetArrayLength(array) - 1] << " }";
						env->ReleaseDoubleArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'C') /* static char[] */
					{
						auto array = reinterpret_cast<jcharArray>(obj);
						jchar* elements = env->GetCharArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						{
							if (elements[j] == '\n')
								stream << R"('\n', )";
							else if (elements[j] == '\r')
								stream << R"('\r', )";
							else
								stream << '\'' << static_cast<char>(elements[j]) << "', ";
						}

						if (elements[env->GetArrayLength(array) - 1] == '\n')
							stream << R"('\n' })";
						else if (elements[env->GetArrayLength(array) - 1] == '\r')
							stream << R"('\r' })";
						else
							stream << '\'' << static_cast<char>(elements[env->GetArrayLength(array) - 1]) << "' }";

						env->ReleaseCharArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).back() == 'Z') /* static boolean[] */
					{
						auto array = reinterpret_cast<jbooleanArray>(obj);
						jboolean* elements = env->GetBooleanArrayElements(array, nullptr);

						for (jint j = 0; j < env->GetArrayLength(array) - 1; j++)
						{
							if (elements[j])
								stream << "true, ";
							else
								stream << "false, ";
						}

						if (elements[env->GetArrayLength(array) - 1])
							stream << "true }";
						else
							stream << "false }";

						env->ReleaseBooleanArrayElements(array, elements, JNI_ABORT);
					}
					else if (std::string(signature).find("[L") != std::string::npos) /* static custom object array */
					{
						auto jarray = reinterpret_cast<jobjectArray>(obj);

						for (jint j = 0; j < env->GetArrayLength(jarray); j++)
						{
							jobject element = env->GetObjectArrayElement(jarray, j);
							auto element_str = reinterpret_cast<jstring>(env->CallObjectMethod(element, toStringMethod));
							const char* element_cstr;

							if (element == nullptr)
								element_cstr = "null";
							else
								element_cstr = env->GetStringUTFChars(element_str, nullptr);

							std::string estr(element_cstr);
							estr = std::regex_replace(estr, std::regex("\n"), "\\n");
							estr = std::regex_replace(estr, std::regex("\r"), "\\r");

							if (j == env->GetArrayLength(jarray) - 1)
								stream << estr << " }";
							else
								stream << estr << ", ";
						}
					}

					if (stream.str().length() <= 2) // empty array
						stream << '}';

					payload.heapByteData.push_back(str + '\a' + stream.str());
				}
				else if (*local_var_table[i].signature == 'L')
				{
					// call java method because jvmti has no suitable function for turning objects into raw bytes
					jmethodID objectToBytesMethod = env->GetStaticMethodID(exception_class, "objectToBytes", "(Ljava/lang/Object;)[B");
					jobject rawByteArray = env->CallStaticObjectMethod(exception_class, objectToBytesMethod, obj);
					std::stringstream stream;
					auto array = reinterpret_cast<jbyteArray>(rawByteArray);
					jbyte* bytes = env->GetByteArrayElements(array, nullptr);

					// if object is already in a string format, no need to generate hex dump
					if (rawByteArray == nullptr || str.find('@') == std::string::npos)
						continue;

					for (jint j = 0; j < env->GetArrayLength(array); j++)
						stream << std::hex << int{ bytes[j] } << ' ';

					payload.heapByteData.push_back(str + '\a' + stream.str());
					env->ReleaseByteArrayElements(array, bytes, JNI_ABORT);
				}
			}
		}
	}

	// write all data gathered from the JVM to shared file
serialize_launch:
	visualizer.serializeDataStruct(payload);
	visualizer.launch();
}

static std::string Agent::dataTypeFormatter(std::string unformatted)
{
	std::string formatted;
	std::ranges::replace(unformatted, '/', '.');

	// if data type is NOT primitive
	if (unformatted.find('L') != std::string::npos)
	{
		// get substring between 'L' and ';' ("Ljava.lang.Object;" becomes "java.lang.Object")
		formatted += unformatted.substr(unformatted.find('L') + 1, unformatted.find(';') - unformatted.find('L') - 1);

		// add bracket pairs for each layer of nested array ("[[[Ljava.lang.Object;" becomes "java.lang.Object[][][]")
		for (const char& ch : unformatted)
			if (ch == '[')
				formatted += "[]";

		return formatted;
	}

	// primitive data types are always the last SINGLE character
	formatted += Agent::mappings.at(unformatted.back());

	// if data type is primitive array
	for (const char& ch : unformatted)
		if (ch == '[')
			formatted += "[]";

	return formatted;
}

static std::string Agent::decodeJVMTypeSignature(const std::string& name, const std::string& signature, bool isMethod)
{
	if (!isMethod)
		return Agent::dataTypeFormatter(signature) + '\a' + name;

	// handle the return type and method name
	std::string decoded;
	decoded += Agent::dataTypeFormatter(signature.substr(signature.find(')'))) + ' ' + name + '(';

	// handle parameters of the method
	std::string params = signature.substr(1, signature.find(')') - 1);
	std::vector<std::string> tokens;

	// extract individual parameters
	for (size_t i = 0; i < params.length(); i++)
	{
		if (params[i] == 'L' || params[i] == '[')
		{
			// keep on adding brackets if the parameter data type is a nested array 
			std::string temp;
			while (params[i] == '[')
				temp += params[i++];

			// no more brackets [left]/[to begin with]
			if (params[i] == 'L')
			{
				// add the fully qualified class name
				while (params[i] != ';')
					temp += params[i++];

				tokens.push_back(temp + ';');
				continue;
			}
			
			tokens.push_back(temp + params[i]);
			continue;
		}

		// a primitive token represented as a single character
		tokens.push_back({params[i]});
	}

	for (const std::string& token : tokens)
		decoded += Agent::dataTypeFormatter(token) + ", ";

	// remove trailing whitespace and comma
	if (decoded.find(", ") != std::string::npos)
	{
		decoded.pop_back();
		decoded.pop_back();
	}
	
	return decoded + ')';
}