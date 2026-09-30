#include "SettingsParser.h"
#include <QFile>
#include <QDomDocument>
#include <zlib.h>
#include <QDebug>
#include <QByteArray>

TaskConfig SettingsParser::parse(const QString &filePath)
{
    TaskConfig config;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open settings file:" << filePath;
        return config;
    }
    
    QByteArray compressedData = file.readAll();
    file.close();

    // Decompress using zlib
    QByteArray xmlData;
    xmlData.resize(compressedData.size() * 10); // heuristic buffer
    
    uLongf destLen = xmlData.size();
    int res = uncompress((Bytef*)xmlData.data(), &destLen, (const Bytef*)compressedData.constData(), compressedData.size());
    
    if (res != Z_OK) {
        qDebug() << "Zlib decompression failed:" << res;
        return config;
    }
    xmlData.resize(destLen);
    
    QDomDocument doc;
    if (!doc.setContent(xmlData)) {
        qDebug() << "Failed to parse XML";
        return config;
    }
    
    QDomElement root = doc.documentElement();
    if (root.tagName() == "ExamInformation") {
        config.name = root.attribute("Name");
        config.inputFile = root.attribute("InputFile");
        config.outputFile = root.attribute("OutputFile");
        config.useStdIn = (root.attribute("UseStdIn") == "true");
        config.useStdOut = (root.attribute("UseStdOut") == "true");
        config.evaluatorName = root.attribute("EvaluatorName");
        config.mark = root.attribute("Mark").toDouble();
        config.timeLimit = root.attribute("TimeLimit").toDouble();
        config.memoryLimit = root.attribute("MemoryLimit").toDouble();
        
        QDomNode n = root.firstChild();
        while(!n.isNull()) {
            QDomElement e = n.toElement();
            if(!e.isNull() && e.tagName() == "TestCase") {
                TestCaseConfig tc;
                tc.name = e.attribute("Name");
                tc.mark = e.attribute("Mark").toDouble();
                tc.timeLimit = e.attribute("TimeLimit").toDouble();
                tc.memoryLimit = e.attribute("MemoryLimit").toDouble();
                config.testCases.append(tc);
            }
            n = n.nextSibling();
        }
    }
    return config;
}

bool SettingsParser::save(const QString &filePath, const TaskConfig &config)
{
    QDomDocument doc;
    QDomElement root = doc.createElement("ExamInformation");
    doc.appendChild(root);

    root.setAttribute("Name", config.name);
    root.setAttribute("InputFile", config.inputFile);
    root.setAttribute("OutputFile", config.outputFile);
    root.setAttribute("UseStdIn", config.useStdIn ? "true" : "false");
    root.setAttribute("UseStdOut", config.useStdOut ? "true" : "false");
    root.setAttribute("EvaluatorName", config.evaluatorName);
    root.setAttribute("Mark", QString::number(config.mark));
    root.setAttribute("TimeLimit", QString::number(config.timeLimit));
    root.setAttribute("MemoryLimit", QString::number(config.memoryLimit));

    for (const TestCaseConfig &tc : config.testCases) {
        QDomElement tcElem = doc.createElement("TestCase");
        tcElem.setAttribute("Name", tc.name);
        if (tc.mark >= 0) tcElem.setAttribute("Mark", QString::number(tc.mark));
        if (tc.timeLimit >= 0) tcElem.setAttribute("TimeLimit", QString::number(tc.timeLimit));
        if (tc.memoryLimit >= 0) tcElem.setAttribute("MemoryLimit", QString::number(tc.memoryLimit));
        root.appendChild(tcElem);
    }

    QByteArray xmlData = doc.toByteArray(-1);
    
    // Compress using zlib
    QByteArray compressedData;
    uLongf destLen = compressBound(xmlData.size());
    compressedData.resize(destLen);
    
    int res = compress((Bytef*)compressedData.data(), &destLen, (const Bytef*)xmlData.constData(), xmlData.size());
    if (res != Z_OK) {
        qDebug() << "Zlib compression failed:" << res;
        return false;
    }
    compressedData.resize(destLen);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Cannot write settings file:" << filePath;
        return false;
    }
    
    file.write(compressedData);
    file.close();

    return true;
}
