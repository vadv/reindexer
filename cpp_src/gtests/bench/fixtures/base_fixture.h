#pragma once

#include <benchmark/benchmark.h>

#include <stddef.h>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#include "allocs_tracker.h"
#include "core/keyvalue/keyarray.h"
#include "core/namespacedef.h"
#include "core/reindexer.h"
#include "sequence.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

using benchmark::State;
using benchmark::internal::Benchmark;

using reindexer::Error;
using reindexer::IndexDef;
using reindexer::Item;
using reindexer::KeyRef;
using reindexer::KeyRefs;
using reindexer::NamespaceDef;
using reindexer::Reindexer;

class BaseFixture {
public:
	virtual ~BaseFixture() {
		assert(db_);
		auto err = db_->CloseNamespace(nsdef_.name);
		if (!err.ok()) {
			std::cerr << "Error while closing namespace '" << nsdef_.name << "'. Reason: " << err.what() << std::endl;
		}
	}

	BaseFixture(Reindexer* db, const string& name, size_t maxItems, size_t idStart = 1)
		: db_(db), nsdef_(name), id_seq_(std::make_shared<Sequence>(idStart, maxItems, 1)) {}

	BaseFixture(Reindexer* db, NamespaceDef& nsdef, size_t maxItems, size_t idStart = 1)
		: db_(db), nsdef_(nsdef), id_seq_(make_shared<Sequence>(idStart, maxItems, 1)) {}

	virtual BaseFixture& AddIndex(const string& name, const string& jsonPath, const string& indexType, const string& fieldType,
								  IndexOpts opts = IndexOpts());

	virtual BaseFixture& AddIndex(const IndexDef& index);

	virtual Error Initialize();

	virtual void RegisterAllCases();

protected:
	void Insert(State& state);
	void Update(State& state);

	virtual Item MakeItem() = 0;

	template <typename Fn, typename Cl>
	Benchmark* Register(const string& name, Fn fn, Cl* cl) {
		return benchmark::RegisterBenchmark((nsdef_.name + "/" + name).c_str(), std::bind(fn, cl, _1));
	}

protected:
	// helper functions

	template <typename Predicate>
	IndexDef* findIndex(State& state, Predicate pred) {
		auto it = std::find_if(nsdef_.indexes.begin(), nsdef_.indexes.end(), pred);
		if (it == nsdef_.indexes.end()) {
			state.SkipWithError("Index for query condition not found");
			return nullptr;
		}
		return it.base();
	}

protected:
	Reindexer* db_;
	NamespaceDef nsdef_;
	shared_ptr<Sequence> id_seq_;
};