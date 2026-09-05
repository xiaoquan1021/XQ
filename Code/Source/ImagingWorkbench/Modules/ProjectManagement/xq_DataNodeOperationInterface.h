#ifndef XQ_DATANODEOPERATIONINTERFACE_H
#define XQ_DATANODEOPERATIONINTERFACE_H

#include <xqProjectManagementExports.h>

#include <mitkOperationActor.h>
#include <mitkDataStorage.h>

class XQPROJECTMANAGEMENT_EXPORT xq_DataNodeOperationInterface : public mitk::OperationActor
{
public:
    xq_DataNodeOperationInterface();
    ~xq_DataNodeOperationInterface() override;

    void SetDataStorage(mitk::DataStorage::Pointer dataStorage);
    mitk::DataStorage::Pointer GetDataStorage() const;

    void ExecuteOperation(mitk::Operation* operation) override;

private:
    mitk::DataStorage::Pointer m_DataStorage;
};

#endif
