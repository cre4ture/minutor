/** Copyright (c) 2013, Sean Kasun */
#ifndef BLOCKIDENTIFIER_H_
#define BLOCKIDENTIFIER_H_

#include "identifierinterface.h"

#include <QString>
#include <QMap>
#include <QHash>
#include <QList>
#include <QColor>

class JSONArray;
class JSONObject;


class BlockInfo {
 public:
  BlockInfo();

  // special block attribute used during mob spawning detection
  bool isOpaque();
  bool isLiquid();
  bool doesBlockHaveSolidTopSurface(int data);
  bool isBlockNormalCube();
  bool renderAsNormalBlock();
  bool canProvidePower();

  // special block type used during mob spawning detection
  bool isBedrock();
  bool isHopper();
  bool isStairs();
  bool isHalfSlab();
  bool isSnow();

  // special blocks with Biome based Grass, Foliage and Water colors
  bool biomeWater();
  bool biomeGrass();
  bool biomeFoliage();

  void setName(const QString &newname);
  void setBiomeGrass(bool value);
  void setBiomeFoliage(bool value);
  const QString &getName();

  int id;
  double alpha;
  quint8 mask;
  bool enabled;
  bool transparent;
  bool liquid;
  bool rendernormal;
  bool providepower;
  bool spawninside;
  QColor colors[16];

 private:
  QString name;
  // cache special blocks used during mob spawning detection
  bool    bedrock;
  bool    hopper;
  bool    stairs;
  bool    halfslab;
  bool    snow;
  bool    water;
  bool    grass;
  bool    foliage;
};

class BlockIdentifier: public IdentifierI {
 public:
  BlockIdentifier();
  ~BlockIdentifier();
  int addDefinitions(JSONArray *, int pack = -1) override;
  void setDefinitionsEnabled(int packId, bool enabled) override;
  BlockInfo &getBlock(int id, int data);
 private:
  void clearCache();
  void parseDefinition(JSONObject *block, BlockInfo *parent, int pack);
  QMap<quint32, QList<BlockInfo *>> blocks;
  QList<QList<BlockInfo*> > packs;
  BlockInfo *cache[65536];
};

#endif  // BLOCKIDENTIFIER_H_
