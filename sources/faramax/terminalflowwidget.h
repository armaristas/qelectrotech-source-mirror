/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FARAMAX_TERMINALFLOWWIDGET_H
#define FARAMAX_TERMINALFLOWWIDGET_H

#include "terminalflowproject.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QTextEdit;

namespace Faramax
{
class TerminalFlowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalFlowWidget(QETProject *project, QWidget *parent = nullptr);
private:
    void scan();
    void preview();
    void apply();
    void invalidate();
    void addRange();
    void showPlan();
    QPointer<QETProject> m_project;
    TerminalFlow::Snapshot m_snapshot;
    TerminalFlow::Plan m_plan;
    QLineEdit *m_identifier = nullptr;
    QCheckBox *m_replace = nullptr, *m_suffixes = nullptr;
    QTabWidget *m_modes = nullptr;
    QTableWidget *m_strips = nullptr, *m_ranges = nullptr, *m_preview = nullptr;
    QTextEdit *m_diagnostics = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_apply = nullptr;
};
}
#endif
