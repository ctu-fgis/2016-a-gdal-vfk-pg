#ifndef GDAL_OGR_VFK_VFKREADERPG_H_INCLUDED
#define GDAL_OGR_VFK_VFKREADERPG_H_INCLUDED

#include "vfkreaderp.h"

/************************************************************************/
/*                              VFKReaderPG                             */
/************************************************************************/

class VFKReaderPG : public VFKReaderDB
{
protected:
    PGconn     *m_poDB;
    PGresult   *m_res;
    const char *m_pszConnStr;

public:
    VFKReaderPG(const char *);
    virtual ~VFKReaderPG();

    OGRErr        ExecuteSQL(const char *, bool = FALSE); // TODO
};

#endif // GDAL_OGR_VFK_VFKREADERPG_H_INCLUDED
