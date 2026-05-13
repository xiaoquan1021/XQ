#ifndef XQ_IMAGEPROCESSINGPLUGIN_H
#define XQ_IMAGEPROCESSINGPLUGIN_H

#include <ctkPluginActivator.h>
#include <XQ_QT_IMAGEPROCESSINGExports.h>

class XQ_QT_IMAGEPROCESSING_EXPORT xq_ImageProcessingPlugin
  : public QObject
  , public ctkPluginActivator
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org_xq_gui_qt_imageprocessing")
  Q_INTERFACES(ctkPluginActivator)

public:
  xq_ImageProcessingPlugin();
  ~xq_ImageProcessingPlugin() override;

  void start(ctkPluginContext* context) override;
  void stop(ctkPluginContext* context) override;
};

#endif
