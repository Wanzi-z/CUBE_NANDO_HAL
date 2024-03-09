/*  Copyright (C) 2020 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "spi_nor_db.h"
#include <cstring>
#include <QDebug>
#include <QMessageBox>

SpiNorDb::SpiNorDb()
{
    readFromCvs();
}

SpiNorDb::~SpiNorDb()
{
}

ChipInfo *SpiNorDb::stringToChipInfo(const QString &s)
{
    int paramNum;
    quint64 paramValue;
    QStringList paramsList;
    SpiNorInfo *ci = new SpiNorInfo();

    paramsList = s.split(',');
    paramNum = paramsList.size();
    if (paramNum != CHIP_PARAM_NUM)
    {
        QMessageBox::critical(nullptr, QObject::tr("错误"),
            QObject::tr("无法正常读取芯片数据库条目. 需要 %2 个参数,数据库存在 %3个参数,请确认数据库参数").arg(CHIP_PARAM_NUM).arg(paramNum));
        delete ci;
        return nullptr;
    }

    ci->setName(paramsList[CHIP_PARAM_NAME]);
    if (getParamFromString(paramsList[CHIP_PARAM_PAGE_SIZE], paramValue))
    {
        QMessageBox::critical(nullptr, QObject::tr("错误"),
            QObject::tr("无法分析参数 %1")
            .arg(paramsList[CHIP_PARAM_PAGE_SIZE]));
        delete ci;
        return nullptr;
    }
    ci->setPageSize(paramValue);

    if (getParamFromString(paramsList[CHIP_PARAM_BLOCK_SIZE], paramValue))
    {
        QMessageBox::critical(nullptr, QObject::tr("错误"),
            QObject::tr("无法分析参数 %1")
            .arg(paramsList[CHIP_PARAM_BLOCK_SIZE]));
        delete ci;
        return nullptr;
    }
    ci->setBlockSize(paramValue);

    if (getParamFromString(paramsList[CHIP_PARAM_TOTAL_SIZE], paramValue))
    {
        QMessageBox::critical(nullptr, QObject::tr("错误"),
            QObject::tr("无法分析参数 %1")
            .arg(paramsList[CHIP_PARAM_TOTAL_SIZE]));
        delete ci;
        return nullptr;
    }
    ci->setTotalSize(paramValue);

    for (int i = CHIP_PARAM_PAGE_OFF; i < CHIP_PARAM_NUM; i++)
    {
        if (getOptParamFromString(paramsList[i], paramValue))
        {
            QMessageBox::critical(nullptr, QObject::tr("错误"),
                QObject::tr("无法分析参数 %1").arg(paramsList[i]));
            delete ci;
            return nullptr;
        }
        ci->setParam(i - CHIP_PARAM_PAGE_OFF, paramValue);
    }

    return ci;
}

int SpiNorDb::chipInfoToString(ChipInfo *chipInfo, QString &s)
{
    QString csvValue;
    QStringList paramsList;
    SpiNorInfo *ci = dynamic_cast<SpiNorInfo *>(chipInfo);

    paramsList.append(ci->getName());
    getStringFromParam(ci->getPageSize(), csvValue);
    paramsList.append(csvValue);
    getStringFromParam(ci->getBlockSize(), csvValue);
    paramsList.append(csvValue);
    getStringFromParam(ci->getTotalSize(), csvValue);
    paramsList.append(csvValue);

    for (int i = CHIP_PARAM_PAGE_OFF; i < CHIP_PARAM_NUM; i++)
    {
        if (getStringFromOptParam(ci->getParam(i - CHIP_PARAM_PAGE_OFF),
            csvValue))
        {
            return -1;
        }
        paramsList.append(csvValue);
    }

    s = paramsList.join(", ");

    return 0;
}

QString SpiNorDb::getDbFileName()
{
    return dbFileName;
}

ChipInfo *SpiNorDb::chipInfoGetByName(QString name)
{
    for(int i = 0; i < chipInfoVector.size(); i++)
    {
        if (!chipInfoVector[i]->getName().compare(name))
            return chipInfoVector[i];
    }

    return nullptr;
}

int SpiNorDb::getIdByChipId(uint32_t id1, uint32_t id2, uint32_t id3,
    uint32_t id4, uint32_t id5)
{
    for(int i = 0; i < chipInfoVector.size(); i++)
    {
        // Mandatory IDs
        if (id1 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID1) ||
            id2 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID2))
        {
            continue;
        }

        // Optinal IDs
        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID3) ==
            ChipDb::paramNotDefValue)
        {
            return i;
        }
        if (id3 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID3))
            continue;

        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID4) ==
            ChipDb::paramNotDefValue)
        {
            return i;
        }
        if (id4 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID4))
            continue;

        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID5) ==
            ChipDb::paramNotDefValue)
        {
            return i;
        }
        if (id5 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID5))
            continue;

        return i;
    }

    return -1;
}

QString SpiNorDb::getNameByChipId(uint32_t id1, uint32_t id2,
    uint32_t id3, uint32_t id4, uint32_t id5, uint32_t id6)
{
    for(int i = 0; i < chipInfoVector.size(); i++)
    {
        // Mandatory IDs
        if (id1 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID1) ||
            id2 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID2))
        {
            continue;
        }

        // Optinal IDs
        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID3) ==
            ChipDb::paramNotDefValue)
        {
            return chipInfoVector[i]->getName();
        }
        if (id3 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID3))
            continue;

        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID4) ==
            ChipDb::paramNotDefValue)
        {
            return chipInfoVector[i]->getName();
        }
        if (id4 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID4))
            continue;

        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID5) ==
            ChipDb::paramNotDefValue)
        {
            return chipInfoVector[i]->getName();
        }
        if (id5 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID5))
            continue;

        if (chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID6) ==
            ChipDb::paramNotDefValue)
        {
            return chipInfoVector[i]->getName();
        }
        if (id6 != chipInfoVector[i]->getParam(SpiNorInfo::CHIP_PARAM_ID6))
            continue;

        return chipInfoVector[i]->getName();
    }

    return QString();
}

quint64 SpiNorDb::getChipParam(int chipIndex, int paramIndex)
{
    SpiNorInfo *ci = dynamic_cast<SpiNorInfo *>(getChipInfo(chipIndex));

    if (!ci || paramIndex < 0)
        return 0;

    return ci->getParam(paramIndex);
}

int SpiNorDb::setChipParam(int chipIndex, int paramIndex,
    quint64 paramValue)
{
    SpiNorInfo *ci = dynamic_cast<SpiNorInfo *>(getChipInfo(chipIndex));

    if (!ci || paramIndex < 0)
        return -1;

    return ci->setParam(paramIndex, paramValue);
}
