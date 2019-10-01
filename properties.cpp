/** Copyright 2014 Rian Shelley */
#include <QRegularExpression>
#include <QVector3D>
#include "./properties.h"
#include "./ui_properties.h"


Properties::Properties(QWidget *parent) : QDialog(parent),
    ui(new Ui::Properties) {
  ui->setupUi(this);
}

PropertieTreeCreator::PropertieTreeCreator()
{
    summary.insert("", "{id} ({Pos.[0]}, {Pos.[1]}, {Pos.[2]})");
    summary.insert("Pos", "({[0]}, {[1]}, {[2]})");
    summary.insert("Attributes[]", "{Name} = {Base}");
}

Properties::~Properties() {
  delete ui;
}


template <class IterableT>
void PropertieTreeCreator::ParseIterable(QTreeWidgetItem* node, const IterableT& seq) {
  typename IterableT::const_iterator it, itEnd = seq.end();
  for (it = seq.begin(); it != itEnd; ++it) {
    QTreeWidgetItem* child = new QTreeWidgetItem();
    child->setData(0, Qt::DisplayRole, it.key());
    child->setData(1, Qt::DisplayRole, GetSummary(it.key(), it.value()));
    CreateTree(child, it.value());
    node->addChild(child);
  }
}


template <class IterableT>
void PropertieTreeCreator::ParseList(QTreeWidgetItem* node, const IterableT& seq) {
  typename IterableT::const_iterator it, itEnd = seq.end();
  int i = 0;
  // skip 1 sized arrays
  if (seq.size() == 0) {
    // empty
    if (node)
      node->setData(1, Qt::DisplayRole, "<empty>");
  } else if (seq.size() == 1) {
    CreateTree(node, seq.first());
    if (node)
      node->setData(1, Qt::DisplayRole, GetSummary("[0]", seq.first()));
  } else {
    for (it = seq.begin(); it != itEnd; ++it) {
      QTreeWidgetItem* child = new QTreeWidgetItem();
      QString key = QString("[%1]").arg(i++);
      child->setData(0, Qt::DisplayRole, key);
      child->setData(1, Qt::DisplayRole, GetSummary(key, *it));
      CreateTree(child, *it);

      node->addChild(child);
    }
  }
}

void Properties::DisplayProperties(QVariant p) {
  // get current property
  QString propertyName;
  QTreeWidgetItem* item = ui->propertyView->currentItem();
  if (item) {
    propertyName = item->data(0, Qt::DisplayRole).toString();
  }

  ui->propertyView->clear();

  // only support QVariantMap or QVariantHash at this level
  switch (p.type()) {
    case QMetaType::QVariantMap:
      m_creator.ParseIterable(ui->propertyView->invisibleRootItem(), p.toMap());
      break;
    case QMetaType::QVariantHash:
      m_creator.ParseIterable(ui->propertyView->invisibleRootItem(), p.toHash());
      break;
    case QMetaType::QVariantList:
      m_creator.ParseList(ui->propertyView->invisibleRootItem(), p.toList());
      break;
    default:
      qWarning("Trying to display scalar value as a property");
      break;
  }

  // expand at least the first level
  ui->propertyView->expandToDepth(0);

  if (propertyName.size() != 0) {
    // try to restore the path
    QList<QTreeWidgetItem*> items =
        ui->propertyView->findItems(propertyName, Qt::MatchRecursive);
    if (items.size())
      items.front()->setSelected(true);
  }
}



void PropertieTreeCreator::CreateTree(QTreeWidgetItem* node, const QVariant& v) {
  switch (v.type()) {
    case QMetaType::QVariantMap:
      ParseIterable(node, v.toMap());
      break;
    case QMetaType::QVariantHash:
      ParseIterable(node, v.toHash());
      break;
    case QMetaType::QVariantList:
      ParseList(node, v.toList());
      break;
    default:
      if (node)
        node->setData(1, Qt::DisplayRole, v.toString());
      break;
  }
}

QString EvaluateSubExpression(const QString& subexpr, const QVariant& v) {
  if (subexpr.size() == 0) {
    // limit the displayed decimal places
    if ((QMetaType::Type)v.type() == QMetaType::Double) {
      return QString::number(v.toDouble(), 'f', 2);
    }
    return v.toString();
  } else if (subexpr.at(0) == '[') {
    int rightbracket = subexpr.indexOf(']');
    if (rightbracket > 0) {
      bool ok = false;
      int index = subexpr.mid(1, rightbracket-1).toInt(&ok);
      if (ok && (QMetaType::Type)v.type() == QMetaType::QVariantList) {
        return EvaluateSubExpression(subexpr.mid(rightbracket + 1),
                                     v.toList().at(index));
      }
    }
  } else {
    int dot = subexpr.indexOf('.');
    QString key = subexpr.mid(0, dot);
    if ((QMetaType::Type)v.type() == QMetaType::QVariantHash) {
      QHash<QString, QVariant> h = v.toHash();
      QHash<QString, QVariant>::const_iterator it = h.find(key);
      if (it != h.end())
        return EvaluateSubExpression(subexpr.mid(key.length() + 1), *it);
    } else if ((QMetaType::Type)v.type() == QMetaType::QVariantMap) {
      QMap<QString, QVariant> h = v.toMap();
      QMap<QString, QVariant>::const_iterator it = h.find(key);
      if (it != h.end())
        return EvaluateSubExpression(subexpr.mid(key.length() + 1), *it);
    }
  }

  return "";
}

QString PropertieTreeCreator::GetSummary(const QString& key, const QVariant& v) {
  QString ret;
  QMap<QString, QString>::const_iterator it = summary.find(key);
  if (it != summary.end()) {
    ret = *it;
    QRegularExpression re("(\\{.*?\\})");
    QRegularExpressionMatchIterator matches = re.globalMatch(*it);
    while (matches.hasNext()) {
      QRegularExpressionMatch m = matches.next();
      QString pattern = m.captured(0);
      QString evaluated =
          EvaluateSubExpression(pattern.mid(1, pattern.size() - 2), v);
      // if a lookup fails, then don't return anything
      if (evaluated.size() == 0) {
        ret = "";
        break;
      }
      ret.replace(pattern, evaluated);
    }
  } else if ((QMetaType::Type)v.type() == QMetaType::QVariantList) {
    ret = QString("(%1 items)").arg(v.toList().size());
  } else if ((QMetaType::Type)v.type() == QMetaType::QVariantMap) {
    ret = GetSummary("", v);
  }
  return ret;
}


void Properties::on_propertyView_itemClicked(QTreeWidgetItem *item, int column)
{
  if (evaluateBB(item))
  {
    return;
  }

  for (int i = 0; i < item->childCount(); i++)
  {
    if (evaluateBB(item->child(i)))
    {
      return;
    }
  }
}

bool Properties::evaluateBB(QTreeWidgetItem *item)
{
  QString text = item->data(0, Qt::DisplayRole).toString();

  if ((text == "BB") && (item->childCount() >= 6))
  {
    QVector3D from;
    from.setX(item->child(0)->data(1, Qt::DisplayRole).toFloat());
    from.setY(item->child(1)->data(1, Qt::DisplayRole).toFloat());
    from.setZ(item->child(2)->data(1, Qt::DisplayRole).toFloat());

    QVector3D to;
    to.setX(item->child(3)->data(1, Qt::DisplayRole).toFloat());
    to.setY(item->child(4)->data(1, Qt::DisplayRole).toFloat());
    to.setZ(item->child(5)->data(1, Qt::DisplayRole).toFloat());

    emit onBoundingBoxSelected(from, to);
    return true;
  }

  return false;
}

void Properties::on_propertyView_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    on_propertyView_itemClicked(current, 0);
}
