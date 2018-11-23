/** Copyright (c) 2013, Sean Kasun */
#ifndef BIOMEIDENTIFIER_H_
#define BIOMEIDENTIFIER_H_

#include "identifierinterface.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QColor>
class JSONArray;


class BiomeInfo {
  // public methods
 public:
  BiomeInfo();
  QColor getBiomeGrassColor  ( QColor blockcolor, int elevation );
  QColor getBiomeFoliageColor( QColor blockcolor, int elevation );
  QColor getBiomeWaterColor( QColor watercolor );

  // public members
 public:
  int id;
  QString name;
  bool enabled;
  QColor colors[16];
  QColor watermodifier;
  bool   enabledwatermodifier;
  double alpha;
  double temperature;
  double humidity;

  // private methods and members
 private:
  typedef struct T_BiomeCorner {
    int red;
    int green;
    int blue;
  } T_BiomeCorner;
  static T_BiomeCorner grassCorners[3];
  static T_BiomeCorner foliageCorners[3];
  static QColor getBiomeColor( float temperature, float humidity, int elevation, T_BiomeCorner *corners );
  static QColor mixColor( QColor colorizer, QColor blockcolor );
};

class BiomeIdentifier: public IdentifierI {
 public:
  BiomeIdentifier();
  ~BiomeIdentifier();
  int addDefinitions(JSONArray *, int pack = -1) override;
  void setDefinitionsEnabled(int id, bool enabled) override;
  BiomeInfo &getBiome(int id);
 private:
  QHash<int, QList<BiomeInfo*>> biomes;
  QList<QList<BiomeInfo*> > packs;
};

#endif  // BIOMEIDENTIFIER_H_
