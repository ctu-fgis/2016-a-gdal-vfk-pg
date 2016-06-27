/******************************************************************************
 * $Id: vfkreaderdb.cpp 33713 2016-03-12 17:41:57Z goatbar $
 *
 * Project:  VFK Reader (DB)
 * Purpose:  Implements VFKReaderDB class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "cpl_vsi.h"

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#include <cstring>

#define SUPPORT_GEOMETRY

#ifdef SUPPORT_GEOMETRY
#include "ogr_geometry.h"
#endif

/*!
  \brief VFKReaderDB constructor
*/
VFKReaderDB::VFKReaderDB(const char *pszFileName) : VFKReader(pszFileName)
{
    // PB: zvazit, zda nechame OGR_VFK_DB_SPATIAL - umoznuje sestavit bez geometrie
    if (CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_SPATIAL", "YES")))
        m_bSpatial = TRUE;    // build geometry from DB
    else
        m_bSpatial = FALSE;   // store also geometry in DB
}

/*!
  \brief VFKReaderDB destructor
*/
VFKReaderDB::~VFKReaderDB()
{
    delete[] m_pszDBname;
}

/*!
  \brief Load data block definitions (&B)

  Call VFKReader::OpenFile() before this function.

  \return number of data blocks or -1 on error
*/

#if 0
int VFKReaderDB::ReadDataBlocks()
{
    int  nDataBlocks = -1;
    CPLString osSQL;
    const char *pszName, *pszDefn;
    IVFKDataBlock *poNewDataBlock;

    sqlite3_stmt *hStmt;

    osSQL.Printf("SELECT table_name, table_defn FROM %s", VFK_DB_TABLE);
    hStmt = PrepareStatement(osSQL.c_str());
    while(ExecuteSQL(hStmt) == OGRERR_NONE) {
        pszName = (const char*) sqlite3_column_text(hStmt, 0);
        pszDefn = (const char*) sqlite3_column_text(hStmt, 1);
        poNewDataBlock = (IVFKDataBlock *) CreateDataBlock(pszName);
        poNewDataBlock->SetGeometryType();
        poNewDataBlock->SetProperties(pszDefn);
        VFKReader::AddDataBlock(poNewDataBlock, NULL);
    }

    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "BEGIN", NULL, NULL, NULL));
    /* Read data from VFK file */
    nDataBlocks = VFKReader::ReadDataBlocks();
    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "COMMIT", NULL, NULL, NULL));

    return nDataBlocks;
}
#endif

/*!
  \brief Load data records (&D)

  Call VFKReader::OpenFile() before this function.

  \param poDataBlock limit to selected data block or NULL for all

  \return number of data records or -1 on error
*/
int VFKReaderDB::ReadDataRecords(IVFKDataBlock *poDataBlock)
{
    int         bReadDb, bReadVfk;
    int         nDataRecords;
    int         iDataBlock;
    const char *pszName;
    CPLString   osSQL;

    IVFKDataBlock *poDataBlockCurrent;

    pszName = NULL;
    nDataRecords = 0;
    bReadVfk = !m_bDbSource;
    bReadDb = FALSE;

    if (poDataBlock) { /* read records only for selected data block */
        /* table name */
        pszName = poDataBlock->GetName();

        /* check for existing records (re-use already inserted data) */
        osSQL.Printf("SELECT num_records FROM %s WHERE "
                     "table_name = '%s'",
                     VFK_DB_TABLE, pszName);
        //hStmt = PrepareStatement(osSQL.c_str());
        nDataRecords = ExecuteSQL(osSQL.c_str());
         if (nDataRecords > 0)
             bReadDb = TRUE; /* -> read from DB */
         else
             nDataRecords = 0;

        /* TODO: 
        }
        catch ... {
            
        }
        
        if (ExecuteSQL(hStmt) == OGRERR_NONE) {
            nDataRecords = sqlite3_column_int(hStmt, 0);
            if (nDataRecords > 0)
                bReadDb = TRUE; -> read from DB 
            else
                nDataRecords = 0;
        }
        */

    }
    else {                     /* read all data blocks */
        /* check for existing records (re-use already inserted data) */
        int count;

        osSQL.Printf("SELECT COUNT(*) FROM %s WHERE num_records > 0", VFK_DB_TABLE);
        if (ExecuteSQL(osSQL.c_str(), count) == OGRERR_NONE && count != 0) {
            bReadDb = TRUE;     /* -> read from DB */
        }

        /* check if file is already registered in DB (requires file_size column) */
        osSQL.Printf("SELECT COUNT(*) FROM %s WHERE file_name = '%s' AND "
                     "file_size = " CPL_FRMT_GUIB " AND num_records > 0",
                     VFK_DB_TABLE, CPLGetFilename(m_pszFilename),
                     (GUIntBig) m_poFStat->st_size);
        if (ExecuteSQL(osSQL.c_str(), count) == OGRERR_NONE && count > 0) {
            /* -> file already registered (filename & size is the same) */
            CPLDebug("OGR-VFK", "VFK file %s already loaded in DB", m_pszFilename);
            bReadVfk = FALSE;
        }
    }
    
    if (bReadDb) {        /* read records from DB */
        /* read from  DB */
        GIntBig iFID;
        int  iRowId;
        VFKFeatureDB *poNewFeature = NULL;

        poDataBlockCurrent = NULL;
        for (iDataBlock = 0; iDataBlock < GetDataBlockCount(); iDataBlock++) {
            poDataBlockCurrent = GetDataBlock(iDataBlock);

            if (poDataBlock && poDataBlock != poDataBlockCurrent)
                continue;

            poDataBlockCurrent->SetFeatureCount(0); /* avoid recursive call */

            pszName = poDataBlockCurrent->GetName();
            CPLAssert(NULL != pszName);

            osSQL.Printf("SELECT %s,_rowid_ FROM %s ",
                         FID_COLUMN, pszName);
            if (EQUAL(pszName, "SBP"))
              osSQL += "WHERE PORADOVE_CISLO_BODU = 1 ";
            osSQL += "ORDER BY ";
            osSQL += FID_COLUMN;
            nDataRecords = 0;
            std::vector<VFKDbValue> record;
            record.push_back(VFKDbValue(DT_BIGINT));
            record.push_back(VFKDbValue(DT_INT));
            PrepareStatement(osSQL.c_str());
            while (ExecuteSQL(record) == OGRERR_NONE) {
                iFID = (int) record[0]; // TODO: static_cast
                iRowId = (int) record[1];
                poNewFeature = new VFKFeatureDB(poDataBlockCurrent, iRowId, iFID);
                poDataBlockCurrent->AddFeature(poNewFeature);
                nDataRecords++;
            }

            /* check DB consistency */
            osSQL.Printf("SELECT num_features FROM %s WHERE table_name = '%s'",
                         VFK_DB_TABLE, pszName);
            {
                int nFeatDB;
                if (ExecuteSQL(osSQL.c_str(), nFeatDB) == OGRERR_NONE &&
                    nFeatDB > 0 && nFeatDB != poDataBlockCurrent->GetFeatureCount())
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: Invalid number of features " CPL_FRMT_GIB " (should be %d)",
                             pszName, poDataBlockCurrent->GetFeatureCount(), nFeatDB);
            }
        }
    }

    if (bReadVfk) {                          /* read from VFK file and insert records into DB */
        /* begin transaction */
        ExecuteSQL("BEGIN");

        /* Store VFK header to DB */
        StoreInfo2DB();
        
        /* Insert VFK data records into DB */
        nDataRecords += VFKReader::ReadDataRecords(poDataBlock);

        /* update VFK_DB_TABLE table */
        poDataBlockCurrent = NULL;
        for (iDataBlock = 0; iDataBlock < GetDataBlockCount(); iDataBlock++) {
            poDataBlockCurrent = GetDataBlock(iDataBlock);

            if (poDataBlock && poDataBlock != poDataBlockCurrent)
                continue;

            osSQL.Printf("UPDATE %s SET num_records = %d WHERE "
                         "table_name = '%s'",
                         VFK_DB_TABLE, poDataBlockCurrent->GetRecordCount(),
                         poDataBlockCurrent->GetName());

            ExecuteSQL(osSQL);
        }

        /* commit transaction */
        ExecuteSQL("COMMIT");
    }

    return nDataRecords;
}

/*!
  \brief Store header info to VFK_DB_HEADER
*/
void VFKReaderDB::StoreInfo2DB()
{
    CPLString osSQL;
    const char *value;
    char q;

    for(std::map<CPLString, CPLString>::iterator i = poInfo.begin();
        i != poInfo.end(); ++i) {
        value = i->second.c_str();

        q = (value[0] == '"') ? ' ' : '"';

        osSQL.Printf("INSERT INTO %s VALUES(\"%s\", %c%s%c)",
                     VFK_DB_HEADER, i->first.c_str(),
                     q, value, q);
        ExecuteSQL(osSQL);
    }
}

/*!
  \brief Create index

  If creating unique index fails, then non-unique index is created instead.

  \param name index name
  \param table table name
  \param column column(s) name
  \param unique TRUE to create unique index
*/
void VFKReaderDB::CreateIndex(const char *name, const char *table, const char *column,
                                  bool unique)
{
    CPLString   osSQL;

    if (unique) {
        osSQL.Printf("CREATE UNIQUE INDEX %s ON %s (%s)",
                     name, table, column);
        if (ExecuteSQL(osSQL.c_str()) == OGRERR_NONE) {
            return;
        }
    }

    osSQL.Printf("CREATE INDEX %s ON %s (%s)",
                 name, table, column);
    ExecuteSQL(osSQL.c_str());
}

/*!
  \brief Create new data block

  \param pszBlockName name of the block to be created

  \return pointer to VFKDataBlockDB instance
*/
IVFKDataBlock *VFKReaderDB::CreateDataBlock(const char *pszBlockName)
{
    /* create new data block, i.e. table in DB */
    return new VFKDataBlockDB(pszBlockName, (IVFKReader *) this);
}

/*!
  \brief Create DB table from VFKDataBlock (SQLITE only)

  \param poDataBlock pointer to VFKDataBlock instance
*/
void VFKReaderDB::AddDataBlock(IVFKDataBlock *poDataBlock, const char *pszDefn)
{
    int count;
    const char *pszBlockName;
    const char *pszKey;
    CPLString   osCommand, osColumn;

    VFKPropertyDefn *poPropertyDefn;

    pszBlockName = poDataBlock->GetName();

    /* register table in VFK_DB_TABLE */
    osCommand.Printf("SELECT COUNT(*) FROM %s WHERE "
                     "table_name = '%s'",
                     VFK_DB_TABLE, pszBlockName);
    if (ExecuteSQL(osCommand.c_str(), count) == OGRERR_NONE && count == 0) {

        osCommand.Printf("CREATE TABLE IF NOT EXISTS '%s' (", pszBlockName);
        for (int i = 0; i < poDataBlock->GetPropertyCount(); i++) {
            poPropertyDefn = poDataBlock->GetProperty(i);
            if (i > 0)
                osCommand += ",";
            osColumn.Printf("%s %s", poPropertyDefn->GetName(),
                            poPropertyDefn->GetTypeSQL().c_str());
            osCommand += osColumn;
        }
        osColumn.Printf(",%s integer", FID_COLUMN);
	osCommand += osColumn;
	if (poDataBlock->GetGeometryType() != wkbNone) {
	    osColumn.Printf(",%s blob", GEOM_COLUMN);
            osCommand += osColumn;
	}
	osCommand += ")";
        ExecuteSQL(osCommand.c_str()); /* CREATE TABLE */

        /* create indices */
        osCommand.Printf("%s_%s", pszBlockName, FID_COLUMN);
        CreateIndex(osCommand.c_str(), pszBlockName, FID_COLUMN,
                    !EQUAL(pszBlockName, "SBP"));

        pszKey = ((VFKDataBlockDB *) poDataBlock)->GetKey();
        if (pszKey) {
            osCommand.Printf("%s_%s", pszBlockName, pszKey);
            CreateIndex(osCommand.c_str(), pszBlockName, pszKey, !m_bAmendment);
        }

        if (EQUAL(pszBlockName, "SBP")) {
            /* create extra indices for SBP */
            CreateIndex("SBP_OB",        pszBlockName, "OB_ID", FALSE);
            CreateIndex("SBP_HP",        pszBlockName, "HP_ID", FALSE);
            CreateIndex("SBP_DPM",       pszBlockName, "DPM_ID", FALSE);
            CreateIndex("SBP_OB_HP_DPM", pszBlockName, "OB_ID,HP_ID,DPM_ID", TRUE);
            CreateIndex("SBP_OB_POR",    pszBlockName, "OB_ID,PORADOVE_CISLO_BODU", FALSE);
            CreateIndex("SBP_HP_POR",    pszBlockName, "HP_ID,PORADOVE_CISLO_BODU", FALSE);
            CreateIndex("SBP_DPM_POR",   pszBlockName, "DPM_ID,PORADOVE_CISLO_BODU", FALSE);
        }
        else if (EQUAL(pszBlockName, "HP")) {
            /* create extra indices for HP */
            CreateIndex("HP_PAR1",        pszBlockName, "PAR_ID_1", FALSE);
            CreateIndex("HP_PAR2",        pszBlockName, "PAR_ID_2", FALSE);
        }
        else if (EQUAL(pszBlockName, "OB")) {
            /* create extra indices for OP */
            CreateIndex("OB_BUD",        pszBlockName, "BUD_ID", FALSE);
        }

        /* update VFK_DB_TABLE meta-table */
        osCommand.Printf("INSERT INTO %s (file_name, file_size, table_name, "
                         "num_records, num_features, num_geometries, table_defn) VALUES "
			 "('%s', " CPL_FRMT_GUIB ", '%s', -1, 0, 0, '%s')",
			 VFK_DB_TABLE, CPLGetFilename(m_pszFilename),
                         (GUIntBig) m_poFStat->st_size,
                         pszBlockName, pszDefn);

        ExecuteSQL(osCommand.c_str());
    }

    return VFKReader::AddDataBlock(poDataBlock, NULL);
}

/*!
  \brief Add feature

  \param poDataBlock pointer to VFKDataBlock instance
  \param poFeature pointer to VFKFeature instance
*/
OGRErr VFKReaderDB::AddFeature(IVFKDataBlock *poDataBlock, VFKFeature *poFeature)
{
    CPLString     osCommand;
    CPLString     osValue;

    const char   *pszBlockName;

    OGRFieldType  ftype;

    const VFKProperty *poProperty;

    VFKFeatureDB *poNewFeature;

    pszBlockName = poDataBlock->GetName();
    osCommand.Printf("INSERT INTO '%s' VALUES(", pszBlockName);

    for (int i = 0; i < poDataBlock->GetPropertyCount(); i++) {
        ftype = poDataBlock->GetProperty(i)->GetType();
        poProperty = poFeature->GetProperty(i);
        if (i > 0)
            osCommand += ",";
        if (poProperty->IsNull())
            osValue.Printf("NULL");
        else {
            switch (ftype) {
            case OFTInteger:
                osValue.Printf("%d", poProperty->GetValueI());
                break;
            case OFTReal:
                osValue.Printf("%f", poProperty->GetValueD());
                break;
            case OFTString:
                if (poDataBlock->GetProperty(i)->IsIntBig())
		    osValue.Printf("%s", poProperty->GetValueS());
                else
                    osValue.Printf("'%s'", poProperty->GetValueS(TRUE));
                break;
            default:
                osValue.Printf("'%s'", poProperty->GetValueS());
                break;
            }
        }
        osCommand += osValue;
    }
    osValue.Printf("," CPL_FRMT_GIB, poFeature->GetFID());
    if (poDataBlock->GetGeometryType() != wkbNone) {
	osValue += ",NULL";
    }
    osValue += ")";
    osCommand += osValue;

    if (ExecuteSQL(osCommand.c_str(), TRUE) != OGRERR_NONE)
        return OGRERR_FAILURE;

    if (EQUAL(pszBlockName, "SBP")) {
        poProperty = poFeature->GetProperty("PORADOVE_CISLO_BODU");
        if( poProperty == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find property PORADOVE_CISLO_BODU");
            return OGRERR_FAILURE;
        }
        if (!EQUAL(poProperty->GetValueS(), "1"))
            return OGRERR_NONE;
    }

    poNewFeature = new VFKFeatureDB(poDataBlock, poDataBlock->GetRecordCount(RecordValid) + 1,
                                        poFeature->GetFID());
    poDataBlock->AddFeature(poNewFeature);

    return OGRERR_NONE;
}
