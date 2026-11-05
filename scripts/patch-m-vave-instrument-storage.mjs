import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const app = path.join(root, "vendor/m-vave-fm1/app");
const libraryPath = path.join(app, "fm_library.c");
const instrumentPath = path.join(app, "instrument.h");

function replaceOnce(source, before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0) {
    throw new Error("FM1-Lab storage patch: expected one exact match for " + label);
  }
  return source.slice(0, first) + after + source.slice(first + before.length);
}

let library = fs.readFileSync(libraryPath, "utf8");
if (!library.includes('#include "fm1lab_bank_metadata.h"')) {
  library = replaceOnce(library, '#include "fm_patch.h"\n',
    '#include "fm_patch.h"\n#include "fm1lab_bank_metadata.h"\n', "library include");
  library = replaceOnce(library,
    'enum { BANK_MAGIC=0x31424d46U, STAGE=GROOVE_PROJECT_SLOTS*4096, PACKED_VOICE=112 };\nstruct bank { uint32_t magic, version, index, crc, commit; uint8_t voice[32][PACKED_VOICE]; };',
    'enum { BANK_MAGIC=0x31424d46U, STAGE=GROOVE_PROJECT_SLOTS*4096, PACKED_VOICE=112, META_BYTES=FM1LAB_BANK_META_BYTES };\nstruct bank { uint32_t magic, version, index, crc, commit; uint8_t voice[32][PACKED_VOICE]; uint8_t metadata[META_BYTES]; };\n_Static_assert(offsetof(struct bank,metadata)==FM1LAB_BANK_META_OFFSET, "Instrument metadata offset");',
    "bank layout");
  library = replaceOnce(library,
    '    return ~groove_crc32(crc,b->voice,sizeof b->voice);\n}',
    '    crc=groove_crc32(crc,b->voice,sizeof b->voice);\n    if (b->version>=2) crc=groove_crc32(crc,b->metadata,sizeof b->metadata);\n    return ~crc;\n}',
    "versioned bank checksum");
  library = replaceOnce(library,
    'if (!b || b->magic!=BANK_MAGIC || b->version!=1 || b->index>=4 || b->commit || b->crc!=checksum(b)) return NULL;',
    'if (!b || b->magic!=BANK_MAGIC || (b->version!=1 && b->version!=2) || b->index>=4 || b->commit || b->crc!=checksum(b)) return NULL;',
    "legacy and v2 bank reader");
  library = replaceOnce(library,
    'int groove_fm_library_write(unsigned index, const uint8_t raw[4096])\n{',
    'static int library_write_internal(unsigned index, const uint8_t raw[4096], const uint8_t metadata[META_BYTES])\n{',
    "bank writer signature");
  library = replaceOnce(library,
    '    b->magic=BANK_MAGIC; b->version=1; b->index=index; b->commit=~0U;\n    for (unsigned i=0;i<32;++i) pack7(b->voice[i],raw+i*128);\n    b->crc=checksum(b);',
    '    b->magic=BANK_MAGIC; b->version=2; b->index=index; b->commit=~0U;\n    for (unsigned i=0;i<32;++i) pack7(b->voice[i],raw+i*128);\n    if (metadata) memcpy(b->metadata,metadata,META_BYTES);\n    else if (banks[index] && banks[index]->version>=2) memcpy(b->metadata,banks[index]->metadata,META_BYTES);\n    else { fm1lab_bank_metadata_t defaults; fm1lab_bank_metadata_init(&defaults);\n        if (fm1lab_bank_metadata_encode(&defaults,b->metadata)) { free(b); return GROOVE_PROJECT_INVALID; } }\n    b->crc=checksum(b);',
    "v2 write and metadata preservation");
  library = replaceOnce(library,
    '    if (!r) r=groove_fm_library_recover();\n    fm1_engine_resume(); return r;\n}',
    '    if (!r) r=groove_fm_library_recover();\n    fm1_engine_resume(); return r;\n}\n\nint groove_fm_library_write(unsigned index, const uint8_t raw[4096])\n{\n    return library_write_internal(index,raw,NULL);\n}\n\nint groove_fm_library_metadata_read(unsigned index, uint8_t metadata[META_BYTES])\n{\n    if (index>=4 || !metadata) return GROOVE_PROJECT_INVALID;\n    if (blocked) { groove_fm_library_init(); if (blocked) return blocked; }\n    if (banks[index] && banks[index]->version>=2) memcpy(metadata,banks[index]->metadata,META_BYTES);\n    else { fm1lab_bank_metadata_t defaults; fm1lab_bank_metadata_init(&defaults);\n        if (fm1lab_bank_metadata_encode(&defaults,metadata)) return GROOVE_PROJECT_INVALID; }\n    return 0;\n}\n\nint groove_fm_library_metadata_write(unsigned index, const uint8_t metadata[META_BYTES])\n{\n    if (index>=4 || !metadata) return GROOVE_PROJECT_INVALID;\n    fm1lab_bank_metadata_t decoded;\n    if (fm1lab_bank_metadata_decode(&decoded,metadata)) return GROOVE_PROJECT_INVALID;\n    uint8_t raw[4096];\n    int r=groove_fm_library_read(index,raw);\n    if (r) return r;\n    return library_write_internal(index,raw,metadata);\n}',
    "public metadata APIs");
  library = replaceOnce(library,
    '    return library_write_internal(index,raw,metadata);\n}',
    '    return library_write_internal(index,raw,metadata);\n}\n\nint groove_fm_library_instrument_write(unsigned index, const uint8_t raw[4096], const uint8_t metadata[META_BYTES])\n{\n    if (index>=4 || !raw || !metadata) return GROOVE_PROJECT_INVALID;\n    fm1lab_bank_metadata_t decoded;\n    if (fm1lab_bank_metadata_decode(&decoded,metadata)) return GROOVE_PROJECT_INVALID;\n    return library_write_internal(index,raw,metadata);\n}',
    "atomic Instrument bank writer");
  fs.writeFileSync(libraryPath, library);
}

let instrument = fs.readFileSync(instrumentPath, "utf8");
if (!instrument.includes("GROOVE_FM_METADATA_BYTES")) {
  instrument = replaceOnce(instrument,
    'enum { GROOVE_PROJECT_SLOTS = 16, GROOVE_PROJECT_BYTES = 4064,\n       GROOVE_PROJECT_LEGACY_BYTES = 4048 };',
    'enum { GROOVE_PROJECT_SLOTS = 16, GROOVE_PROJECT_BYTES = 4064,\n       GROOVE_PROJECT_LEGACY_BYTES = 4048, GROOVE_FM_METADATA_BYTES = 400 };\n/* Eight four-patch Instrument records live in the unused tail of each DX7 bank sector. */\nint groove_fm_library_metadata_read(unsigned index, uint8_t metadata[GROOVE_FM_METADATA_BYTES]);\nint groove_fm_library_metadata_write(unsigned index, const uint8_t metadata[GROOVE_FM_METADATA_BYTES]);\nint groove_fm_library_instrument_write(unsigned index, const uint8_t raw[4096], const uint8_t metadata[GROOVE_FM_METADATA_BYTES]);',
    "public storage declarations");
  fs.writeFileSync(instrumentPath, instrument);
}

console.log("Applied FM1-Lab versioned Instrument bank storage integration.");
