#include "xq_MultiPhysicsView.h"

#include <QLabel>
#include <QVBoxLayout>

const QString xq_MultiPhysicsView::VIEW_ID = "org.xq.views.multiphysics";

xq_MultiPhysicsView::xq_MultiPhysicsView()
  : m_StatusLabel(nullptr)
{
}

xq_MultiPhysicsView::~xq_MultiPhysicsView() = default;

void xq_MultiPhysicsView::CreateQtPartControl(QWidget* parent)
{
  auto* layout = new QVBoxLayout(parent);

  m_StatusLabel = new QLabel(
      "Multi-Physics Simulation\n\n"
      "Coupled multi-physics simulation workflow.\n\n"
      "Data model: xq_MultiPhysicsJob (ready)\n"
      "XML export: xq_MultiPhysicsXmlWriter (ready)\n"
      "Test: test_multiphysics_job_xml (passing)\n\n"
      "GUI workflow steps (planned):\n"
      "  1. Define computational domains\n"
      "  2. Assign material properties per domain\n"
      "  3. Configure equations and solver settings\n"
      "  4. Set boundary conditions with parameters\n"
      "  5. Export svFSI XML input file\n"
      "  6. Run coupled solver\n"
      "  7. Import and visualize results\n\n"
      "Status: Data layer complete. Full GUI under development.",
      parent);
  m_StatusLabel->setWordWrap(true);
  m_StatusLabel->setStyleSheet("QLabel { padding: 16px; }");
  layout->addWidget(m_StatusLabel);
  layout->addStretch();
}

void xq_MultiPhysicsView::SetFocus()
{
  if (m_StatusLabel)
    m_StatusLabel->setFocus();
}
