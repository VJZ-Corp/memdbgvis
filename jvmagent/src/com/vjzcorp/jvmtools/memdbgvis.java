package com.vjzcorp.jvmtools;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.FileWriter;
import java.io.ByteArrayOutputStream;
import java.io.ObjectOutputStream;
import java.io.NotSerializableException;
import java.lang.management.ManagementFactory;
import java.util.List;

/**
 * This class is a custom exception that serves as an interface between the Java debugee code and the visualizer.
 * Its main purpose is to invoke the native C++ agent which is responsible for communicating with the JVM and visualizer.
 *
 * @author VJZ
 * @version 1.0.0
 */
public final class memdbgvis extends Exception {
    /**
     * Method used for invoking the agent and subsequently launching the visualizer. It accomplishes this in two steps:
     * <br><br>
     * 1. Fetch the line number of invocation and write it to a shared data file.
     * <br>
     * 2. Launch the visualizer by throwing a special exception which is handled by the native C++ JVMTI agent.
     */
    public static void visualize() {
        // getting JVM arguments
        final List<String> args = ManagementFactory.getRuntimeMXBean().getInputArguments();

        // find the visualizer agent argument
        for (String arg : args) {
            if (arg.startsWith("-agentpath")) {
                String path = arg.substring(arg.indexOf(':') + 1).replaceAll(".dll", ".dat");
                try (PrintWriter writer = new PrintWriter(new FileWriter(path, false))) {
                    writer.println(new memdbgvis().getStackTrace()[1].getLineNumber()); // write line number to shared file
                } catch (Exception e) {
                    e.printStackTrace();
                }

                try {
                    try {
                        throw new memdbgvis();  // invoke the agent by throwing and then immediately catching an exception named 'memdbgvis'
                    } catch (memdbgvis e) {
                        return;
                    }
                } catch (NullPointerException e) {
                    return;
                }
            }
        }
    }

    /**
     * Utility method that retrieves runtime and memory metrics from different managed beans.
     * @see java.lang.management.MemoryMXBean
     * @see java.lang.management.MemoryUsage
     * @see java.lang.management.ThreadMXBean
     * @see Runtime
     * @return a string with one metric per line
     */
    private static String getRuntimeMetrics() {
        final long free = Runtime.getRuntime().freeMemory() >>> 10;
        final long total = Runtime.getRuntime().totalMemory() >>> 10;
        final String heapUsage = "Heap Usage: " + (ManagementFactory.getMemoryMXBean().getHeapMemoryUsage().getUsed() >>> 10) + " KiB\n";
        final String nonHeapUsage = "Non-Heap Usage: " + (ManagementFactory.getMemoryMXBean().getNonHeapMemoryUsage().getUsed() >>> 10) + " KiB\n";
        final String freeMem = "Free Memory: " + free + " KiB\n";
        final String totalMem = "Total Memory: " + total + " KiB\n";
        final String usedMem = "Memory Usage: " + String.format("%.2f", (float)(total - free) / total * 100) + "%\n";
        final String uptime = "Execution Time: " + ManagementFactory.getThreadMXBean().getCurrentThreadUserTime() / 1e6 + " ms\n";
        final String threadCount = "Live Thread Count: " +  ManagementFactory.getThreadMXBean().getThreadCount();
        return heapUsage + nonHeapUsage + freeMem + totalMem + usedMem + uptime + threadCount;
    }

    /**
     * Special hidden method called from the native JVMTI agent which will serialize any object to a byte array.
     * @param obj The object to be serialized (should come from the native agent directly).
     * @return A byte array that contains the raw contents of 'obj'.
     */
    private static byte[] objectToBytes(Object obj) {
        try {
            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
            new ObjectOutputStream(byteArrayOutputStream).writeObject(obj);
            return byteArrayOutputStream.toByteArray();
        } catch (NotSerializableException e) {
            return "ERROR: Object is not serializable.".getBytes();
        } catch (IOException e) {
            return "Object serialization failed due to an I/O error.".getBytes();
        }
    }
}