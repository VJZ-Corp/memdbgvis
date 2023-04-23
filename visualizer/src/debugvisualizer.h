#pragma once

#ifndef DEBUGVISUALIZER_H
#define DEBUGVISUALIZER_H

#include <QtWidgets>
#include "ui_debugvisualizer.h"

typedef struct
{
    int lineNum;
    QString threadName;
    QString threadPriority;
    QString metrics;
    QVector<QString> methodNames;
    QVector<QString> localVars;
    QVector<QString> staticFields;
    QMap<QString, QString> heapByteMap;
} VisualizerPayload;

class DebugVisualizer final : public QMainWindow
{
    Q_OBJECT

public:
    explicit DebugVisualizer(QWidget *parent = Q_NULLPTR);
    ~DebugVisualizer() Q_DECL_OVERRIDE Q_DECL_EQ_DEFAULT;
    void deserializePayloadData();
    void populateCallStackThreadView();
    void populateLocalVarTable();
    void populateStaticFieldTable();

private slots:
    void onInspectButtonClicked();

private:
    Ui::DebugVisualizerClass ui{};
    VisualizerPayload m_agentData;
};

#endif // DEBUGVISUALIZER_H