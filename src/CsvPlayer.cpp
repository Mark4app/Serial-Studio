/*
 * Copyright (c) 2020-2021 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <QtMath>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <qtcsv/reader.h>

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>

#include "Logger.h"
#include "CsvPlayer.h"
#include "JsonParser.h"
#include "SerialManager.h"

/*
 * Only instance of the class
 */
static CsvPlayer *INSTANCE = nullptr;

/**
 * Shows a macOS-like message box with the given properties
 */
static int NiceMessageBox(QString text, QString informativeText,
                          QString windowTitle = qAppName(),
                          QMessageBox::StandardButtons buttons
                          = QMessageBox::Ok)
{
    auto icon
        = QPixmap(":/images/icon.png")
              .scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QMessageBox box;
    box.setIconPixmap(icon);
    box.setWindowTitle(windowTitle);
    box.setStandardButtons(buttons);
    box.setText("<h3>" + text + "</h3>");
    box.setInformativeText(informativeText);

    return box.exec();
}

CsvPlayer::CsvPlayer()
{
    connect(this, SIGNAL(playerStateChanged()), this, SLOT(updateData()));
    LOG_INFO() << "Initialized CSV Player module";
}

CsvPlayer *CsvPlayer::getInstance()
{
    if (!INSTANCE)
        INSTANCE = new CsvPlayer;

    return INSTANCE;
}

bool CsvPlayer::isOpen() const
{
    return m_csvFile.isOpen();
}

qreal CsvPlayer::progress() const
{
    return ((qreal)framePosition()) / frameCount();
}

bool CsvPlayer::isPlaying() const
{
    return m_playing;
}

QString CsvPlayer::filename() const
{
    if (isOpen())
    {
        auto fileInfo = QFileInfo(m_csvFile.fileName());
        return fileInfo.fileName();
    }

    return "";
}

int CsvPlayer::frameCount() const
{
    return m_csvData.count() - 1;
}

int CsvPlayer::framePosition() const
{
    return m_framePos;
}

QString CsvPlayer::timestamp() const
{
    return m_timestamp;
}

void CsvPlayer::play()
{
    m_playing = true;
    emit playerStateChanged();
}

void CsvPlayer::pause()
{
    m_playing = false;
    emit playerStateChanged();
}

void CsvPlayer::toggle()
{
    m_playing = !m_playing;
    emit playerStateChanged();
}

void CsvPlayer::openFile()
{
    // Check that manual JSON mode is activaded
    auto opMode = JsonParser::getInstance()->operationMode();
    auto jsonOpen = !JsonParser::getInstance()->jsonMapData().isEmpty();
    if (opMode != JsonParser::kManual || !jsonOpen)
    {
        NiceMessageBox(tr("Invalid configuration for CSV player"),
                       tr("You need to select a JSON map file in order to use "
                          "this feature"));
        return;
    }

    // Get file name
    auto file = QFileDialog::getOpenFileName(Q_NULLPTR, tr("Select CSV file"),
                                             QDir::homePath(),
                                             tr("CSV files (*.csv)"));

    // File name empty, abort
    if (file.isEmpty())
        return;

    // Close previous file
    closeFile();

    // Serial device is connected, warn user & disconnect
    auto sm = SerialManager::getInstance();
    if (sm->connected())
    {
        LOG_INFO() << "Serial device open, asking user what to do...";
        auto response
            = NiceMessageBox(tr("Serial port open, do you want to continue?"),
                             tr("In order to use this feature, its necessary "
                                "to disconnect from the serial port"),
                             qAppName(), QMessageBox::No | QMessageBox::Yes);
        if (response == QMessageBox::Yes)
            sm->disconnectDevice();
        else
            return;
    }

    // Try to open the current file
    m_csvFile.setFileName(file);
    LOG_INFO() << "Trying to open CSV file...";
    if (m_csvFile.open(QIODevice::ReadOnly))
    {
        // Read CSV file into string matrix
        LOG_INFO() << "CSV file read, processing CSV data...";
        m_csvData = QtCSV::Reader::readToList(m_csvFile);

        // Validate CSV file
        LOG_INFO() << "CSV frame count" << frameCount();
        LOG_INFO() << "Validating CSV file...";
        bool valid = true;
        for (int i = 1; i < frameCount(); ++i)
        {
            valid &= validateRow(i);
            if (!valid)
                break;
        }
        LOG_INFO() << "CSV valid:" << valid;

        // Read first row & update UI
        if (valid)
        {
            updateData();
            emit openChanged();
        }

        // Show error to the user
        else
        {
            NiceMessageBox(
                tr("There is an error with the data in the CSV file"),
                tr("Please verify that the CSV file was created with Serial "
                   "Studio"));
        }
    }

    // Open error
    else
    {
        LOG_INFO() << "CSV file read error" << m_csvFile.errorString();
        NiceMessageBox(tr("Cannot read CSV file"),
                       tr("Please check file permissions & location"));
        closeFile();
    }
}

void CsvPlayer::closeFile()
{
    m_framePos = 0;
    m_model.clear();
    m_csvFile.close();
    m_csvData.clear();
    m_playing = false;
    m_datasetIndexes.clear();
    m_timestamp = "--.--";

    emit openChanged();
    emit timestampChanged();

    LOG_INFO() << "CSV file closed";
}

void CsvPlayer::nextFrame()
{
    if (framePosition() < frameCount())
    {
        ++m_framePos;
        updateData();
    }
}

void CsvPlayer::previousFrame()
{
    if (framePosition() > 0)
    {
        --m_framePos;
        updateData();
    }
}

void CsvPlayer::setProgress(const qreal progress)
{
    // Ensure that progress value is between 0 and 1
    auto validProgress = progress;
    if (validProgress > 1)
        validProgress = 1;
    else if (validProgress < 0)
        validProgress = 0;

    // Pause player to avoid messing the scheduled timer (if playing)
    if (isPlaying())
        pause();

    // Calculate frame position & update data
    m_framePos = qCeil(frameCount() * validProgress);
    if (validProgress == 0)
        m_framePos = 0;
    else if (validProgress == 1)
        m_framePos = frameCount();

    // Update CSV values
    updateData();
}

void CsvPlayer::updateData()
{
    // File not open, abort
    if (!isOpen())
        return;

    // Update timestamp string
    m_timestamp = getCellValue(framePosition() + 1, 0);
    emit timestampChanged();

    // Construct JSON from CSV & instruct the parser to use this document as
    // input source for the QML bridge
    auto json = getJsonFrame(framePosition() + 1);
    JsonParser::getInstance()->setJsonDocument(json);

    // If the user wants to 'play' the CSV, get time difference between this
    // frame and the next frame & schedule an automated update
    if (isPlaying())
    {
        // Get first frame
        if (framePosition() < frameCount())
        {
            bool error = false;
            auto currTime = getCellValue(framePosition() + 1, 0, &error);
            auto nextTime = getCellValue(framePosition() + 2, 0, &error);

            // No error, calculate difference & schedule update
            if (!error)
            {
                auto format = "yyyy/MMM/dd/ HH:mm:ss::zzz";
                auto currDateTime = QDateTime::fromString(currTime, format);
                auto nextDateTime = QDateTime::fromString(nextTime, format);
                auto msecsToNextF = currDateTime.msecsTo(nextDateTime);
                QTimer::singleShot(msecsToNextF, Qt::PreciseTimer, this,
                                   SLOT(nextFrame()));
            }

            // Error - pause playback
            else
            {
                pause();
                LOG_INFO() << "Error getting timestamp difference";
            }
        }

        // Pause at end of CSV
        else
        {
            pause();
            LOG_INFO() << "CSV playback finished";
        }
    }
}

bool CsvPlayer::validateRow(const int position)
{
    // Ensure that position is valid
    if (m_csvData.count() <= position)
        return false;

    // Get titles & value list
    auto titles = m_csvData.at(0);
    auto list = m_csvData.at(position);

    // Check that row value count is the same
    if (titles.count() != list.count())
    {
        LOG_WARNING() << "Mismatched CSV data on frame" << framePosition();
        closeFile();
        return false;
    }

    // Check that this CSV is valid by checking the time title, this value must
    // be the same one that is used in Export.cpp
    auto rxTitle = "RX Date/Time";
    if (titles.first() != rxTitle)
    {
        LOG_WARNING() << "Invalid CSV file (title format does not match)";
        closeFile();
        return false;
    }

    // Valid row
    return true;
}

QJsonDocument CsvPlayer::getJsonFrame(const int row)
{
    // Create group & dataset model from CSV file
    if (m_model.isEmpty())
    {
        LOG_INFO() << "Generating group/dataset model from CSV...";

        auto titles = m_csvData.at(0);
        for (int i = 1; i < titles.count(); ++i)
        {
            // Construct group string
            QString group;
            auto title = titles.at(i);
            auto glist = title.split(")");
            for (int j = 0; j < glist.count() - 1; ++j)
                group.append(glist.at(j));

            // Remove the '(' from group name
            if (!group.isEmpty())
                group.remove(0, 1);

            // Get dataset name & remove units
            QString dataset = glist.last();
            if (dataset.endsWith("]"))
            {
                while (!dataset.endsWith("["))
                    dataset.chop(1);
            }

            // Remove extra spaces from dataset
            while (dataset.startsWith(" "))
                dataset.remove(0, 1);
            while (dataset.endsWith(" ") || dataset.endsWith("["))
                dataset.chop(1);

            // Register group with dataset map
            if (!m_model.contains(group))
            {
                QSet<QString> set;
                set.insert(dataset);
                m_model.insert(group, set);
            }

            // Update existing group/dataset model
            else if (!m_model.value(group).contains(dataset))
            {
                auto set = m_model.value(group);
                if (!set.contains(dataset))
                    set.insert(dataset);

                m_model.remove(group);
                m_model.insert(group, set);
            }

            // Register dataset index
            if (!m_datasetIndexes.contains(dataset))
                m_datasetIndexes.insert(dataset, i);
        }

        LOG_INFO() << "Group/dataset model created successfully";
    }

    // Read CSV row & JSON template from JSON parser
    auto values = m_csvData.at(row);
    auto mapData = JsonParser::getInstance()->jsonMapData();
    QJsonDocument jsonTemplate = QJsonDocument::fromJson(mapData.toUtf8());

    // Replace JSON title
    auto json = jsonTemplate.object();
    json["t"] = tr("Replay of %1").arg(filename());

    // Replace values in JSON  with values in row using the model.
    // This is very ugly code, somebody please fix it :(
    auto groups = json.value("g").toArray();
    foreach (auto groupKey, m_model.keys())
    {
        for (int i = 0; i < groups.count(); ++i)
        {
            auto group = groups.at(i).toObject();

            if (group.value("t") == groupKey)
            {
                auto datasetKeys = m_model.value(groupKey);
                auto datasets = group.value("d").toArray();
                foreach (auto datasetKey, datasetKeys)
                {
                    for (int j = 0; j < datasets.count(); ++j)
                    {
                        auto dataset = datasets.at(j).toObject();
                        if (dataset.value("t") == datasetKey)
                        {
                            auto value
                                = values.at(m_datasetIndexes.value(datasetKey));
                            dataset.remove("v");
                            dataset.insert("v", value);
                        }

                        datasets.replace(j, dataset);
                    }
                }

                group.remove("d");
                group.insert("d", datasets);
            }

            groups.replace(i, group);
        }
    }

    // Update groups from JSON
    json.remove("g");
    json.insert("g", groups);

    // Return new JSON document
    return QJsonDocument(json);
}

QString CsvPlayer::getCellValue(int row, int cell, bool *error)
{
    if (m_csvData.count() > row)
    {
        auto list = m_csvData.at(row);
        if (list.count() > cell)
        {
            if (error)
                *error = false;

            return list.at(cell);
        }
    }

    if (error)
        *error = true;

    return "";
}
