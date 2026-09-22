#ifndef CHOOCHOO_MODEL_CATALOG_H
#define CHOOCHOO_MODEL_CATALOG_H

#include "selection_popup.h"
#include "project_instruments.h"

extern const SelectionItem plaitsCategories[];
extern const int plaitsCategoryCount;
extern const SelectionItem plaitsAltCategories[];
extern const int plaitsAltCategoryCount;
extern const SelectionItem braidsCategories[];
extern const int braidsCategoryCount;
extern const SelectionItem drumSynthCategories[];
extern const int drumSynthCategoryCount;
extern const SelectionItem mmeCategories[];
extern const int mmeCategoryCount;
extern const SelectionItem sinteredCategories[];
extern const int sinteredCategoryCount;

bool modelCatalogsValid();
const char* modelCatalogName(InstrumentType type, int value);

#endif
