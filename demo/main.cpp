#include "Log.h"
#include "MixSdk.h"
#include "Util.h"
#include "FlvHelper.h"
#include "FlvFile.h"
#include "Job.h"
#include "InFlvStream.h"

#include "json/json.h"

#include <string>
#include <thread>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>

using namespace hercules;
using std::cout;
using std::cerr;
using std::endl;
using std::set;
using std::vector;
using std::string;
using std::shared_ptr;
using std::ifstream;

std::fstream outFs;
std::string outFileName = "output.flv";

void onFlvData(const AVData& data)
{
    static bool writeHeader = false;
    if (!writeHeader)
    {
        string flvFileHeader;
        flvhelper::genDefFLVFileHeader(flvFileHeader);

        logInfo(MIXLOG << "write flv header: " << bin2str(flvFileHeader));
        outFs.open(outFileName, std::ios_base::out | std::ios_base::binary);
        outFs.write(flvFileHeader.data(), flvFileHeader.size());

        writeHeader = true;
    }
    outFs.write(data.m_data.data(), data.m_data.size());
};

void usage(const char* errmsg)
{
    cerr << errmsg << endl;
    cerr << "usage:" << endl;
    cerr << "   ./mixdemo configPath timeInSeconds" << endl;
}

void logCallback(const std::string& s)
{
    std::cout << s << std::endl;
}

bool readJson(const string& configFile, Json::Value& root)
{
    ifstream fs(configFile);
    if (!fs)
    {
        logErr(MIXLOG << configFile << " open file failed.");
        return false;
    }

    Json::Reader reader;
    if (!reader.parse(fs, root, false))
    {
        logErr(MIXLOG << configFile << " read json failed. REASON:"
               << reader.getFormattedErrorMessages());
        return false;
    }
    return true;
}

void updateJson(uint32_t timeout, MixTask* task, Json::Value root)
{
    time_t initTime = time(nullptr);
    while (time(nullptr) < initTime + timeout)
    {
        sleep(5);

        // 更新动画的时间
        for (auto iter = root["input_stream_list"].begin();
                iter != root["input_stream_list"].end();
                ++iter)
        {
            Json::Value& val = *iter;
            if (val["type"].asString().find("animation") != string::npos)
            {
                val["current_time"] = static_cast<uint32_t>((time(nullptr) - initTime) * 1000);
            }
        }

        Json::FastWriter writer;
        std::string json = writer.write(root);
        logInfo(MIXLOG << "JSON:" << json);
        task->updateJson(json);
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        usage("missing argument");
        return 1;
    }

    // 初始化管理器，设置日志回调
    MixTaskManager::getInstance()->init("../data/font/msyhl.ttc", logCallback);

    // 从文件中读取json
    const char* configFilePath = argv[1];
    Json::Value root;
    if (!readJson(configFilePath, root))
    {
        logErr(MIXLOG << "read json from " << configFilePath << " failed.");
        return 1;
    }

    string taskId = root["task_id"].asString();
    logInfo(MIXLOG << "task: " << taskId << " started.");

    // 生成json string
    Json::FastWriter writer;
    std::string json = writer.write(root);
    logInfo(MIXLOG << "JSON:" << json);

    // 基于json创建task 并设置回调
    outFileName = root["out_stream"]["dst"].asString();
    MixTask* task = MixTaskManager::getInstance()->addTask(json, onFlvData);
    if (task == nullptr)
    {
        logErr(MIXLOG << "task init failed");
        return 1;
    }

    // 读取json中指定的输入flv文件
    set<string> streamNames;
    vector< shared_ptr<InFlvStream> > inputStreams;
    for (auto iter = root["input_stream_list"].begin();
            iter != root["input_stream_list"].end();
            ++iter)
    {
        Json::Value& val = *iter;

        string streamname = val["stream_name"].asString();
        string srcFile = val["src"].asString();

        // 只有流需要处理数据
        if (streamname.empty())
            continue;

        // 不能有相同的流名称
        if (streamNames.count(streamname))
        {
            logErr(MIXLOG << "input_stream_list must not has same streamname: " << streamname);
            return 1;
        }
        streamNames.insert(streamname);

        // demo只支持flv文件
        if (srcFile.substr(srcFile.size() - 3) != "flv")
        {
            logErr(MIXLOG << "input_stream_list streamname:" << streamname
                   << "src: " << srcFile
                   << "not a flv file. ignored.");
            continue;
        }

        inputStreams.push_back(std::make_shared<InFlvStream>(streamname, srcFile));
    }

    // 定时发送json，因为任务是10s超时的
    std::thread t(updateJson, atoi(argv[2]), task, root);

    // 开始解析flv，向task喂数据
    logInfo(MIXLOG << "feeding data to task");
    vector<AVData> dataBuffer;
    do
    {
        dataBuffer.clear();
        for (int i = 0; i < inputStreams.size(); ++i)
        {
            static AVData data;
            if (inputStreams[i]->getAVData(data))
            {
                dataBuffer.push_back(data);
            }
        }

        for (int i = 0; i < dataBuffer.size(); ++i)
        {
            task->addAVData(dataBuffer[i]);
        }

    } while (!dataBuffer.empty());

    t.join();

    // 轮询判断任务完成
    while (!task->canStop())
    {
        usleep(1000000);
    }
    logInfo(MIXLOG << "task done");

    // 停止任务，并释放资源
    MixTaskManager::getInstance()->stopTask(taskId); //task_id in json
    task->join();
    delete task;
    return 0;
}
