#ifndef XQ_LEGACYNODEMIGRATION_H
#define XQ_LEGACYNODEMIGRATION_H

#include <xqProjectManagementExports.h>

#include <mitkDataStorage.h>

class XQPROJECTMANAGEMENT_EXPORT xq_LegacyNodeMigration
{
public:
    static int UpgradeImportedLegacyNodes(mitk::DataStorage* dataStorage);

    // Re-parent every node carrying an "xq.pipeline.stage" property into the
    // matching category folder (Paths/Segmentations/Models/Meshes/Simulations)
    // of its project. Nodes already attached to the correct folder are left
    // alone. Returns the number of re-parented nodes. This is a one-shot
    // compatibility fix for projects whose data was saved before the pipeline
    // services started using FindCategoryFolder.
    static int ReparentIntoCategoryFolders(mitk::DataStorage* dataStorage);
};

#endif
