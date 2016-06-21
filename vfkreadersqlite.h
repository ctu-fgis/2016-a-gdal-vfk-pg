#ifndef GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED

#include<vector>

#include "vfkreaderp.h"

#include "sqlite3.h"

/************************************************************************/
/*                              VFKReaderSQLite                         */
/************************************************************************/

class VFKReaderSQLite : public VFKReaderDB
{
private:
    OGRErr        ExecuteSQL(sqlite3_stmt *);
    sqlite3_stmt *m_hStmt;

protected:
    sqlite3 *m_poDB;

public:
    VFKReaderSQLite(const char *);
    virtual ~VFKReaderSQLite();

    void          PrepareStatement(const char *);
    OGRErr        ExecuteSQL(const char *, bool = FALSE);
    OGRErr        ExecuteSQL(const char *, int&);
    OGRErr        ExecuteSQL(std::vector<int>&);
};

#endif // GDAL_OGR_VFK_VFKREADERSQLITE_H_INCLUDED
