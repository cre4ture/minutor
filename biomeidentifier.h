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
  QColor getBiomeGrassColor  ( QColor blockcolor, int elevation ) const;
  QColor getBiomeFoliageColor( QColor blockcolor, int elevation ) const;
  QColor getBiomeWaterColor( QColor watercolor ) const;

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
  // singleton: access to global usable instance
  static BiomeIdentifier &Instance();

  int addDefinitions(JSONArray *, int pack = -1) override;
  void setDefinitionsEnabled(int id, bool enabled) override;
  void updateBiomeDefinition();
  const BiomeInfo &getBiome(int id) const;

private:
  // singleton: prevent access to constructor and copyconstructor
  BiomeIdentifier();
  ~BiomeIdentifier();
  BiomeIdentifier(const BiomeIdentifier &);
  BiomeIdentifier &operator=(const BiomeIdentifier &);

  QHash<int, BiomeInfo*>    biomes;   // consolidated Biome mapping
  QList<QList<BiomeInfo*> > packs;    // raw data of all available packs
};

#endif  // BIOMEIDENTIFIER_H_
