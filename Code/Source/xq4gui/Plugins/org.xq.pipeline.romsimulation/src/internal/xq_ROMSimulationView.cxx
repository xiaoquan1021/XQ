#include "xq_ROMSimulationView.h"

#include <QLabel>
#include <QVBoxLayout>

const QString xq_ROMSimulationView::VIEW_ID = "org.xq.views.romsimulation";

xq_ROMSimulationView::xq_ROMSimulationView()
  : m_StatusLabel(nullptr)
{
}

xq_ROMSimulationView::~xq_ROMSimulationView() = default;

void xq_ROMSimulationView::CreateQtPartControl(QWidget* parent)
{
  auto* layout = new QVBoxLayout(parent);

  m_StatusLabel = new QLabel(
      "ROM Simulation\n\n"
      "Reduced-Order Model (ROM) simulation workflow.\n\n"
      "Data model: xq_ROMSimulationJob (ready)\n"
      "XML export: xq_ROMSimJobXmlWriter (ready)\n"
      "Test: test_rom_job_contract (passing)\n\n"
      "GUI workflow steps (planned):\n"
      "  1. Select model + centerline\n"
      "  2. Configure ROM mesh parameters\n"
      "  3. Set boundary conditions\n"
      "  4. Export solver input files\n"
      "  5. Run ROM solver\n"
      "  6. Import and convert results\n\n"
      "Status: Data layer complete. Full GUI under development.",
      parent);
  m_StatusLabel->setWordWrap(true);
  m_StatusLabel->setStyleSheet("QLabel { padding: 16px; }");
  layout->addWidget(m_StatusLabel);
  layout->addStretch();
}

void xq_ROMSimulationView::SetFocus()
{
  if (m_StatusLabel)
    m_StatusLabel->setFocus();
}
