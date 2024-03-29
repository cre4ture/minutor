#include "chunk.h"
#include "searchblockpluginwidget.h"
#include "searchresultwidget.h"
#include "ui_searchblockpluginwidget.h"
#include "searchtextwidget.h"

#include <set>
#include <algorithm>

SearchBlockPluginWidget::SearchBlockPluginWidget(const SearchBlockPluginWidgetConfigT& config)
  : QWidget(config.parent)
    , ui(new Ui::SearchBlockPluginWidget)
    , m_config(config)
{
  ui->setupUi(this);

  auto idList = m_config.blockIdentifier->getKnownIds();

  QStringList nameList;
  nameList.reserve(idList.size());

  for (auto id: idList)
  {
    auto blockInfo = m_config.blockIdentifier->getBlockInfo(id);
    nameList.push_back(blockInfo.getName());
  }

  nameList.sort(Qt::CaseInsensitive);

  ui->verticalLayout->addWidget(stw_blockId = new SearchTextWidget("block id"));
  ui->verticalLayout->addWidget(stw_blockName = new SearchTextWidget("block name"));

  for (auto name: nameList)
  {
    stw_blockName->addSuggestion(name);
  }
}

SearchBlockPluginWidget::~SearchBlockPluginWidget()
{
  delete ui;
}

QWidget &SearchBlockPluginWidget::getWidget()
{
  return *this;
}

bool SearchBlockPluginWidget::initSearch()
{
  m_searchForIds.clear();

  if (stw_blockId->isActive())
  {
    bool ok = true;
    m_searchForIds.insert(stw_blockId->getSearchText().toUInt());
    if (!ok)
    {
      return false;
    }
  }

  if (stw_blockName->isActive())
  {
    auto idList = m_config.blockIdentifier->getKnownIds();
    for (auto id: idList)
    {
      auto blockInfo = m_config.blockIdentifier->getBlockInfo(id);
      if (stw_blockName->matches(blockInfo.getName()))
      {
        m_searchForIds.insert(id);
      }
    }
  }

  return (m_searchForIds.size() > 0);
}

SearchPluginI::ResultListT SearchBlockPluginWidget::searchChunk(Chunk &chunk)
{
  SearchPluginI::ResultListT results;

  if (m_searchForIds.size() == 0)
  {
    return results;
  }

  for (int z = 0; z < 16; z++)
  {
    for (int y = 0; y < 256; y++)
    {
      for (int x = 0; x < 16; x++)
      {
        const Block bi = chunk.getBlockData(x,y,z);
        const auto it = m_searchForIds.find(bi.id);
        if (it != m_searchForIds.end())
        {
          auto info = m_config.blockIdentifier->getBlockInfo(bi.id);

          SearchResultItem item;
          item.name = info.getName() + " (" + QString::number(bi.id) + ")";
          item.pos = QVector3D(chunk.getChunkX() * 16 + x, y, chunk.getChunkZ() * 16 + z) + QVector3D(0.5,0.5,0.5); // mark center of block, not origin
          item.entity = QSharedPointer<Entity>::create(item.pos);
          results.push_back(item);
        }
      }
    }
  }

  return results;
}
