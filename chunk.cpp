/** Copyright (c) 2013, Sean Kasun */

#include <algorithm>

#include "./chunk.h"
#include "./flatteningconverter.h"
#include "./blockidentifier.h"

quint16 getBits(const unsigned char *data, int pos, int n) {
//  quint16 result = 0;
  int arrIndex = pos/8;
  int bitIndex = pos%8;
  quint32 loc =
    data[arrIndex]   << 24 |
    data[arrIndex+1] << 16 |
    data[arrIndex+2] << 8  |
    data[arrIndex+3];

  return ((loc >> (32-bitIndex-n)) & ((1 << n) -1));
}



Chunk::Chunk() {
  loaded = false;
}

Chunk::~Chunk() {
  if (loaded) {
    for (int i = 0; i < 16; i++)
      if (sections[i]) {
        if (sections[i]->paletteLength > 0) {
          delete[] sections[i]->palette;
        }
        sections[i]->paletteLength = 0;
        sections[i]->palette = NULL;

        delete sections[i];
        sections[i] = NULL;
      }
  }
}


void Chunk::load(const NBT &nbt) {
  renderedAt = -1;  // impossible.
  renderedFlags = 0;  // no flags
  for (int i = 0; i < 16; i++)
    this->sections[i] = NULL;
  highest = 0;

  int version = 0;
  if (nbt.has("DataVersion"))
    version = nbt.at("DataVersion")->toInt();
  const Tag * level = nbt.at("Level");
  chunkX = level->at("xPos")->toInt();
  chunkZ = level->at("zPos")->toInt();

  // load Biome per column
  if (level->has("Biomes")) {
    const Tag_Int_Array * biomes = dynamic_cast<const Tag_Int_Array*> (level->at("Biomes"));
    if ((version >= 1519) && biomes) {
      // raw copy Biome data
      memcpy(this->biomes, biomes->toIntArray(), sizeof(int)*biomes->length());
    } else {
      const Tag * biomes = level->at("Biomes");
      // convert quint8 to quint32
      auto rawBiomes = biomes->toByteArray();
      for (int i=0; i<256; i++)
        this->biomes[i] = rawBiomes[i];
    }
  } else {
    // no Biome data present
    for (int i=0; i<256; i++)
      this->biomes[i] = -1;
  }

  // load available Sections
  if (level->has("Sections")) {
  auto sections = level->at("Sections");
  int numSections = sections->length();
    // loop over all stored Sections, they are not guarantied to be ordered or consecutive
    for (int s = 0; s < numSections; s++) {
      const Tag * section = sections->at(s);
      int idx = section->at("Y")->toInt();
      // only sections 0..15 contain block data
      if ((idx >=0) && (idx <16)) {
        ChunkSection *cs = new ChunkSection();
        if (version >= 1519) {
          loadSection1519(cs, section);
        } else {
          loadSection1343(cs, section);
        }

        this->sections[idx] = cs;
      }
    }
  }

  // parse Structures that start in this Chunk
  if (version >= 1519) {
    if (level->has("Structures")) {
      auto nbtListStructures = level->at("Structures");
      auto structurelist     = GeneratedStructure::tryParseChunk(nbtListStructures);
      for (auto it = structurelist.begin(); it != structurelist.end(); ++it) {
        emit structureFound(*it);
      }
    }
  }

  loaded = true;

  // parse Entities
  if (level->has("Entities")) {
  auto entitylist = level->at("Entities");
  int numEntities = entitylist->length();
  for (int i = 0; i < numEntities; ++i) {
    auto e = Entity::TryParse(entitylist->at(i));
    if (e)
      entities.insertMulti(e->type(), e);
  }
  }

  // check for the highest block in this chunk
  // todo: use highmap from stored NBT data
  for (int i = 15; i >= 0; i--) {
    if (this->sections[i]) {
      for (int j = 4095; j >= 0; j--) {
        if (this->sections[i]->blocks[j]) {
          highest = i * 16 + (j >> 8);
          return;
        }
      }
    }
  }
}

// supported DataVersions:
//    0 = 1.8 and below
//
//  169 = 1.9
//  175 = 1.9.1
//  176 = 1.9.2
//  183 = 1.9.3
//  184 = 1.9.4
//
//  510 = 1.10
//  511 = 1.10.1
//  512 = 1.10.2
//
//  819 = 1.11
//  921 = 1.11.1
//  922 = 1.11.2
//
// 1139 = 1.12
// 1241 = 1.12.1
// 1343 = 1.12.2
//
// 1519 = 1.13
// 1628 = 1.13.1
void Chunk::loadSection1343(ChunkSection *cs, const Tag *section) {
  // copy raw data
  quint8 blocks[4096];
  quint8 data[2048];
  memcpy(blocks, section->at("Blocks")->toByteArray(), 4096);
  memcpy(data,   section->at("Data")->toByteArray(),   2048);
  memcpy(cs->blockLight, section->at("BlockLight")->toByteArray(), 2048);

  // convert old BlockID + data into virtual ID
  for (int i = 0; i < 4096; i++) {
    int d = data[i>>1];         // get raw data (two nibbles)
    if (i & 1) d >>= 4;         // get one nibble of data
    cs->blocks[i] = blocks[i] | ((d & 0x0f) << 8);
  }

  // parse optional "Add" part for higher block IDs in mod packs
  if (section->has("Add")) {
    auto raw = section->at("Add")->toByteArray();
    for (int i = 0; i < 2048; i++) {
      cs->blocks[i * 2] |= (raw[i] & 0xf) << 8;
      cs->blocks[i * 2 + 1] |= (raw[i] & 0xf0) << 4;
      }
  }

  // link to Converter palette
  cs->paletteLength = 0;
  cs->palette = FlatteningConverter::Instance().getPalette();
}

Block Chunk::getBlockData(int x, int y, int z) const
{
    Block result;

    int offset = (x & 0xf) + (z & 0xf) * 16;

    int sec = y >> 4;
    const ChunkSection * const section = sections[sec];
    if (!section) {
        return result;
    int yoffset = (y & 0xf) << 8;
    int data = section->data[(offset + yoffset) / 2];
    if (x & 1) data >>= 4;
    result.id = section->blocks[offset + yoffset];
    result.bd = data & 0xf;

    return result;
}

// Chunk format after "The Flattening" version 1509
void Chunk::loadSection1519(ChunkSection *cs, const Tag *section) {
  BlockIdentifier &bi = BlockIdentifier::Instance();
  // decode Palette to be able to map BlockStates
  if (section->has("Palette")) {
    auto rawPalette = section->at("Palette");
    cs->paletteLength = rawPalette->length();
    cs->palette = new PaletteEntry[cs->paletteLength];
    for (int j = 0; j < rawPalette->length(); j++) {
      // get name and hash it to hid
      cs->palette[j].name = rawPalette->at(j)->at("Name")->toString();
      uint hid  = qHash(cs->palette[j].name);
      // copy all other properties
      if (rawPalette->at(j)->has("Properties"))
      cs->palette[j].properties = rawPalette->at(j)->at("Properties")->getData().toMap();

      // check vor variants
      BlockInfo const & block = bi.getBlockInfo(hid);
      if (block.hasVariants()) {
      // test all available properties
      for (auto key : cs->palette[j].properties.keys()) {
        QString vname = cs->palette[j].name + ":" + key + ":" + cs->palette[j].properties[key].toString();
        uint vhid = qHash(vname);
        if (bi.hasBlockInfo(vhid))
          hid = vhid; // use this vaiant instead
        }
      }
      // store hash of found variant
      cs->palette[j].hid  = hid;
    }
  } else {
    // create a dummy palette
    cs->palette = new PaletteEntry[1];
    cs->palette[0].name = "minecraft:air";
    cs->palette[0].hid  = 0;
  }

  // map BlockStates to BlockData
  // todo: bit fidling looks very complicated -> find easier code
  if (section->has("BlockStates")) {
    auto raw = section->at("BlockStates")->toLongArray();
    int blockStatesLength = section->at("BlockStates")->length();
    unsigned char *byteData = new unsigned char[8*blockStatesLength];
    memcpy(byteData, raw, 8*blockStatesLength);
    std::reverse(byteData, byteData+(8*blockStatesLength));
    int bitSize = (blockStatesLength)*64/4096;
    for (int i = 0; i < 4096; i++) {
      cs->blocks[4095-i] = getBits(byteData, i*bitSize, bitSize);
    }
    delete byteData;
  } else {
    // set everything to 0 (minecraft:air)
    memset(cs->blocks, 0, sizeof(cs->blocks));
  }

    // copy Light data
//  if (section->has("SkyLight")) {
//    memcpy(cs->skyLight, section->at("SkyLight")->toByteArray(), 2048);
//  }
  if (section->has("BlockLight")) {
    memcpy(cs->blockLight, section->at("BlockLight")->toByteArray(), 2048);
  }
}


const PaletteEntry & ChunkSection::getPaletteEntry(int x, int y, int z) {
  int xoffset = x;
  int yoffset = (y & 0x0f) << 8;
  int zoffset = z << 4;
  return palette[blocks[xoffset + yoffset + zoffset]];
}

const PaletteEntry & ChunkSection::getPaletteEntry(int offset, int y) {
  int yoffset = (y & 0x0f) << 8;
  return palette[blocks[offset + yoffset]];
}

//quint8 ChunkSection::getSkyLight(int x, int y, int z) {
//  int xoffset = x;
//  int yoffset = (y & 0x0f) << 8;
//  int zoffset = z << 4;
//  int value = skyLight[(xoffset + yoffset + zoffset) / 2];
//  if (x & 1) value >>= 4;
//  return value & 0x0f;
//}

//quint8 ChunkSection::getSkyLight(int offset, int y) {
//  int yoffset = (y & 0x0f) << 8;
//  int value = skyLight[(offset + yoffset) / 2];
//  if (offset & 1) value >>= 4;
//  return value & 0x0f;
//}

quint8 ChunkSection::getBlockLight(int x, int y, int z) {
  int xoffset = x;
  int yoffset = (y & 0x0f) << 8;
  int zoffset = z << 4;
  int value = blockLight[(xoffset + yoffset + zoffset) / 2];
  if (x & 1) value >>= 4;
  return value & 0x0f;
}

quint8 ChunkSection::getBlockLight(int offset, int y) {
  int yoffset = (y & 0x0f) << 8;
  int value = blockLight[(offset + yoffset) / 2];
  if (offset & 1) value >>= 4;
  return value & 0x0f;
}
