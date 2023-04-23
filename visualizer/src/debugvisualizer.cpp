#include "debugvisualizer.h"

DebugVisualizer::DebugVisualizer(QWidget *parent)
    : QMainWindow(parent)
{
    // the constructor is responsible for setting up UI, connecting button events, deserializing payload, and populating views 
    this->ui.setupUi(this);
    connect(this->ui.pushButton, SIGNAL(clicked()), SLOT(onInspectButtonClicked()));
    connect(this->ui.learnMoreThreads, &QCommandLinkButton::clicked, this, [this] { QMessageBox::information(this, "Memory Debug Visualizer", "In computer science, a thread is a sequential flow of instructions for the processor to execute. Many basic programs utilize a single thread. For example, a program that repeatedly adds numbers will have just one thread dedicated to it. Nowadays, it is common for an application to have multiple threads. For example, a web browser may have a thread dedicated to rendering videos while another thread may be used to download files in the background without interruption."); });
    connect(this->ui.learnMoreObjRef, &QCommandLinkButton::clicked, this, [this] { QMessageBox::information(this, "Memory Debug Visualizer", "In Java, the heap is broken down into pieces and chunks in memory. Unlike the stack, which is contiguous, the heap is often fragmented. As a result, the JVM will not know where an object's data is located without a reference pointing to it. In the local variable table view, object reference values are displayed as a string returned by Object::toString. If you want a more thorough examination of a certain object, navigate to the 'Heap Inspection' tab."); });
    this->deserializePayloadData();
    this->populateCallStackThreadView();
    this->populateLocalVarTable();
    this->populateStaticFieldTable();
    this->ui.localVarTableWidget->resizeColumnsToContents();
    this->ui.staticFieldsTable->resizeColumnsToContents();
}

void DebugVisualizer::deserializePayloadData()
{
    // setup and initialize input file stream
    QFile input_datafile(QCoreApplication::applicationDirPath() + "/memdbgvis.dat");

    // deserialize and load payload into member struct
    if (input_datafile.open(QIODevice::ReadOnly))
    {
	    unsigned data_section_idx = 0;
	    QTextStream input_textstream(&input_datafile);
        this->m_agentData.lineNum = input_textstream.readLine().toInt();
        this->m_agentData.threadName = input_textstream.readLine();
        this->m_agentData.threadPriority = input_textstream.readLine();

        // seven lines of JVM metrics
        for (short i = 0; i < 7; i++)
			this->m_agentData.metrics += input_textstream.readLine() + '\n';

        while (!input_textstream.atEnd())
        {
            // readLine() must only be called once each iteration; otherwise, it may advance to the next line
            QString cur_line = input_textstream.readLine();

            // advance to the next section
            if (cur_line == "SECTION_END_BEGIN_NEW")
            {
                data_section_idx++;
                continue; // advance to the next line as to not process the delimiter
            }

            /*
			 * DATA SECTION index for deserializing file contents to the payload struct.
			 * 1 : Call Stack Data
			 * 2 : Local Variable Data
			 * 3 : Class Field Data
			 * default : Heap Object Data
			 */
        	switch (data_section_idx)
        	{
			case 1:
				this->m_agentData.methodNames.push_back(cur_line);
				continue;
			case 2:
				this->m_agentData.localVars.push_back(cur_line);
        		continue;
            case 3:
                this->m_agentData.staticFields.push_back(cur_line);
                continue;
			default:
                QStringList components = cur_line.split('\a');
                this->m_agentData.heapByteMap[components[0]] = components[1];
        	}
        }
    }
}

void DebugVisualizer::populateCallStackThreadView()
{
    // populate the thread and metrics view
    this->ui.threadNameView->setText(this->m_agentData.threadName + '\n' + this->m_agentData.threadPriority);
    this->ui.lineNum->display(this->m_agentData.lineNum);
	this->ui.runtimeMetricsView->setText(this->m_agentData.metrics);

    // populate call stack view
    for (QString& method_name : this->m_agentData.methodNames)
        this->ui.callStackWidget->addItem(method_name);

    // emphasize the current stack frame
    this->ui.callStackWidget->item(0)->setFont(QFont("Consolas", this->ui.callStackWidget->font().pointSize(), QFont::Bold));
    this->ui.callStackWidget->item(0)->setBackground(Qt::yellow);
}

void DebugVisualizer::populateLocalVarTable()
{
	QTableWidget* local_var_table = this->ui.localVarTableWidget;

    for (QString& var : this->m_agentData.localVars)
    {
        QStringList var_components = var.split('\a');
        local_var_table->insertRow(local_var_table->rowCount());

        // display the unicode view rather than the raw byte
        if (var_components[0] == "char")
        {
            if (var_components[2].toUShort() == '\n') // escape new line
                var_components[2] = R"('\n')";
            else if (var_components[2].toUShort() == '\r')
                var_components[2] = R"('\r')";
            else
                var_components[2] = '\'' + QChar::fromUcs2(var_components[2].toUShort()) + '\'';
        }

        // display true or false rather than 1 or 0
        if (var_components[0] == "boolean") 
            var_components[2] = var_components[2] == '1' ? "true" : "false";

        for (int i = 0; i < 3; i++)
        {
            local_var_table->setItem(local_var_table->rowCount() - 1, i, new QTableWidgetItem(var_components[i]));
            if (var_components[1] == "this") /* give special highlighting to 'this' reference */
                local_var_table->item(local_var_table->rowCount() - 1, i)->setBackground(Qt::cyan);
        }
    }
}

void DebugVisualizer::populateStaticFieldTable()
{
    for (QString& global : this->m_agentData.staticFields)
    {
        QStringList components = global.split('\a');
        this->ui.staticFieldsTable->insertRow(this->ui.staticFieldsTable->rowCount());

        // display the unicode view rather than the raw byte
        if (components[0] == "static char" || components[0] == "char")
        {
            if (components[2].toUShort() == '\n')
                components[2] = R"('\n')";
            else if (components[2].toUShort() == '\r')
                components[2] = R"('\r')";
            else
				components[2] = '\'' + QChar::fromUcs2(components[2].toUShort()) + '\'';
        }

        // display true or false rather than 1 or 0
        if (components[0] == "static boolean" || components[0] == "boolean") 
            components[2] = components[2] == '1' ? "true" : "false";

        for (int i = 0; i < 3; i++)
            this->ui.staticFieldsTable->setItem(this->ui.staticFieldsTable->rowCount() - 1, i, new QTableWidgetItem(components[i]));
    }
}

void DebugVisualizer::onInspectButtonClicked()
{
    // user input
	const QString ref_code = this->ui.plainTextEdit->toPlainText();

    // object reference code not found
    if (this->m_agentData.heapByteMap[ref_code] == nullptr)
    {
        this->ui.textBrowser->setText("Invalid object reference! Make sure you are taking object reference codes from the 'Local Variables' and 'Static Fields' tabs ONLY.");
        return;
    }

    // display array references
	if (this->m_agentData.heapByteMap[ref_code].contains('{'))
	{
		this->ui.textBrowser->setText(this->m_agentData.heapByteMap[ref_code]);
        return;
	}

    // handle hex dump formatting for custom single object references (not arrays)
    QString hexdump, formatted_str, formatted;
    QVector<QChar> unicodes;
    bool isSpace = false;
    QStringList bytes = this->m_agentData.heapByteMap[ref_code].split(' ');

    // group the bytes into 2 hex digits
    for (const QString& byte_grouping : bytes)
    {
        // if the malformed byte cluster is odd in length, add a leading 0
        if (byte_grouping.length() & 1)
            formatted_str += '0' + byte_grouping + ' ';
        else
        {
            for (qsizetype i = 0; i < byte_grouping.length(); i++)
            {
                if (i % 2 == 0 && i > 0)
                    formatted_str += ' ';
                formatted_str += byte_grouping[i];
            }
            formatted_str += ' ';
        }
    }

    // strip extra whitespaces
    for (const QChar& ch : formatted_str)
    {
	    if (ch == ' ')
	    {
		    if (!isSpace)
		    {
                formatted += ch;
                isSpace = true;
		    }
	    }
        else
        {
            formatted += ch;
            isSpace = false;
        }
    }

    // list of formatted bytes in groupings of 2 hex digits
    bytes = formatted.trimmed().split(' ');

    for (qsizetype i = 0; i < bytes.length(); i++)
    {
        if (i % 10 == 0 && i > 0)
        {
            hexdump += "\t|";
            for (qsizetype j = 0; j < 10; j++)
                hexdump += unicodes[j] + ' ';
            hexdump += "|\n";
            unicodes.clear();
        }

        hexdump += bytes[i] + ' ';

        if (bytes[i].toInt(nullptr, 16) < 32)
            unicodes.emplace_back('.');
        else
			unicodes.emplace_back(QChar(bytes[i].toInt(nullptr, 16)).unicode());
    }

    // add spaces to the end for padding
    hexdump += QString(30 - bytes.length() % 10 * 3, ' ') + "\t|";
    for (const auto& unicode : unicodes)
	    hexdump += unicode + ' ';

    // add spaces to the end of unicode view
    hexdump += QString(20 - unicodes.size() * 2, ' ') + '|';
    this->ui.textBrowser->setText(hexdump);
}