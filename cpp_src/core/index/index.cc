#include "index.h"
#include "core/namespacedef.h"
#include "indexordered.h"
#include "indextext/fastindextext.h"
#include "indextext/fuzzyindextext.h"
#include "tools/logger.h"

// Heuristic value of maximum allowed queries
// to an index (without any updates of the index)
// before performing commit.
const int kMaxQueriesBeforeCommit = 5;

namespace reindexer {

Index::Index(const IndexDef& idef, const PayloadType payloadType, const FieldsSet& fields)
	: type_(idef.Type()), name_(idef.name_), opts_(idef.opts_), payloadType_(payloadType), fields_(fields), rawQueriesCount_(0) {
	logPrintf(LogTrace, "Index::Index ('%s',%s,%s)  %s%s%s", idef.name_.c_str(), idef.indexType_.c_str(), idef.fieldType_.c_str(),
			  idef.opts_.IsPK() ? ",pk" : "", idef.opts_.IsDense() ? ",dense" : "", idef.opts_.IsArray() ? ",array" : "");
}

Index::Index(const Index& obj)
	: type_(obj.type_),
	  name_(obj.name_),
	  sortOrders_(obj.sortOrders_),
	  sortId_(obj.sortId_),
	  opts_(obj.opts_),
	  payloadType_(obj.payloadType_),
	  fields_(obj.fields_),
	  rawQueriesCount_(obj.rawQueriesCount_.load()),
	  keyType_(obj.keyType_),
	  selectKeyType_(obj.selectKeyType_) {}

Index::~Index() {}

Index* Index::New(const IndexDef& idef, const PayloadType payloadType, const FieldsSet& fields) {
	switch (idef.Type()) {
		case IndexStrBTree:
		case IndexIntBTree:
		case IndexDoubleBTree:
		case IndexInt64BTree:
		case IndexCompositeBTree:
			return IndexOrdered_New(idef, payloadType, fields);
		case IndexStrHash:
		case IndexIntHash:
		case IndexInt64Hash:
		case IndexCompositeHash:
			return IndexUnordered_New(idef, payloadType, fields);
		case IndexIntStore:
		case IndexStrStore:
		case IndexInt64Store:
		case IndexDoubleStore:
		case IndexBool:
			return IndexStore_New(idef, payloadType, fields);
		case IndexFastFT:
		case IndexCompositeFastFT:
			return FastIndexText_New(idef, payloadType, fields);
		case IndexFuzzyFT:
		case IndexCompositeFuzzyFT:
			return FuzzyIndexText_New(idef, payloadType, fields);
		default:
			throw Error(errParams, "Ivalid index type %d for index '%s'", idef.Type(), idef.name_.c_str());
	}
}

void Index::resetQueriesCountTillCommit() { rawQueriesCount_ = 0; }
bool Index::allowedToCommit(int phases) const {
	return (phases & CommitContext::MakeSortOrders) || (this->rawQueriesCount_ >= kMaxQueriesBeforeCommit);
}

}  // namespace reindexer
