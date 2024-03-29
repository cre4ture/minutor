#include "searchentitypluginwidget.h"
#include "ui_searchentitypluginwidget.h"

#include "./careeridentifier.h"
#include "chunk.h"
#include "searchresultwidget.h"
#include "searchtextwidget.h"

SearchEntityPluginWidget::SearchEntityPluginWidget(const SearchEntityPluginWidgetConfigT& config)
  : QWidget(config.parent)
    , ui(new Ui::SearchEntityPluginWidget)
    , m_config(config)
{
  ui->setupUi(this);

  ui->verticalLayout->addWidget(stw_sells = new SearchTextWidget("sells"));
  ui->verticalLayout->addWidget(stw_buys = new SearchTextWidget("buys"));
  ui->verticalLayout->addWidget(stw_entityType = new SearchTextWidget("entity type"));
  ui->verticalLayout->addWidget(stw_villagerType = new SearchTextWidget("villager type"));
  ui->verticalLayout->addWidget(stw_special = new SearchTextWidget("special"));

  for (const auto& id: m_config.definitions.careerDefinitions->getKnownIds())
  {
    auto desc = m_config.definitions.careerDefinitions->getDescriptor(id);
    stw_villagerType->addSuggestion(desc.name);
  }
}

SearchEntityPluginWidget::~SearchEntityPluginWidget()
{
  delete ui;
}

QWidget &SearchEntityPluginWidget::getWidget()
{
  return *this;
}

SearchPluginI::ResultListT SearchEntityPluginWidget::searchChunk(Chunk &chunk)
{
  SearchPluginI::ResultListT results;

  const auto& map = chunk.getEntityMap();

  for(const auto& e: map)
  {
    EntityEvaluator evaluator(
      EntityEvaluatorConfig(m_config.definitions,
                            results,
                            "",
                            e,
                            std::bind(&SearchEntityPluginWidget::evaluateEntity, this, std::placeholders::_1)
                            )
      );
  }

  return results;
}

bool SearchEntityPluginWidget::evaluateEntity(EntityEvaluator &entity)
{
  bool result = true;

  if (stw_villagerType->isActive())
  {
    QString searchFor = stw_villagerType->getSearchText();
    QString career = entity.getCareerName();
    result = result && (career.contains(searchFor, Qt::CaseInsensitive));
  }

  if (stw_buys->isActive())
  {
    result = result && findBuyOrSell(entity, *stw_buys, 0);
  }

  if (stw_sells->isActive())
  {
    result = result && findBuyOrSell(entity, *stw_sells, 1);
  }

  if (stw_entityType->isActive())
  {
    QString id = entity.getTypeId();
    result = result && stw_entityType->matches(id);
  }

  if (stw_special->isActive())
  {
    result = result && stw_special->matches(entity.getSpecialParams());
  }

  return result;
}

bool SearchEntityPluginWidget::findBuyOrSell(EntityEvaluator &entity, SearchTextWidget& searchText, int index)
{
  bool foundBuy = false;
  auto offers = entity.getOffers();
  for (const auto& offer: offers)
  {
    auto splitOffer = offer.split(" => ");
    foundBuy = (splitOffer.count() > index) && searchText.matches(splitOffer[index]);
    if (foundBuy)
    {
      break;
    }
  }

  return foundBuy;
}


