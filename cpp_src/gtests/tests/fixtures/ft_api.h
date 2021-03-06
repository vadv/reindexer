#pragma once
#include <limits>
#include "reindexer_api.h"
#include "unordered_map"
using std::unordered_map;

class FTApi : public ReindexerApi {
public:
	void SetUp() {
		reindexer.reset(new Reindexer);
		Error err;

		err = reindexer->OpenNamespace("nm1");
		ASSERT_TRUE(err.ok()) << err.what();

		err = reindexer->OpenNamespace("nm2");
		ASSERT_TRUE(err.ok()) << err.what();

		DefineNamespaceDataset(
			"nm1",
			{IndexDeclaration{"id", "hash", "int", IndexOpts().PK()}, IndexDeclaration{"ft1", "text", "string", IndexOpts()},
			 IndexDeclaration{"ft2", "text", "string", IndexOpts()},
			 IndexDeclaration{
				 "ft1+ft2=ft3", "text", "composite",
				 IndexOpts().SetConfig(
					 R"xxx({"enable_translit": true,"enable_numbers_search": true,"enable_kb_layout": true,"merge_limit": 20000,"log_level": 1,"max_step_size": 5000})xxx")}});
		DefineNamespaceDataset(
			"nm2",
			{IndexDeclaration{"id", "hash", "int", IndexOpts().PK()}, IndexDeclaration{"ft1", "text", "string", IndexOpts()},
			 IndexDeclaration{"ft2", "text", "string", IndexOpts()}, IndexDeclaration{"ft1+ft2=ft3", "text", "composite", IndexOpts()}});
	}

	void FillData(int64_t count) {
		for (int i = 0; i < count; ++i) {
			Item item = NewItem(default_namespace);
			item["id"] = counter_;
			auto ft1 = RandString();

			counter_++;

			item["ft1"] = ft1;

			Upsert(default_namespace, item);
			Commit(default_namespace);
		}
	}
	void Add(const std::string& ft1, const std::string& ft2) {
		Add("nm1", ft1, ft2);
		Add("nm2", ft1, ft2);
	}

	std::pair<string, int> Add(const std::string& ft1) {
		Item item = NewItem("nm1");
		item["id"] = counter_;
		counter_++;
		item["ft1"] = ft1;

		Upsert("nm1", item);
		Commit("nm1");
		return make_pair(ft1, item.GetID());
	}
	void Add(const std::string& ns, const std::string& ft1, const std::string& ft2) {
		Item item = NewItem(ns);
		item["id"] = counter_;
		++counter_;
		item["ft1"] = ft1;
		item["ft2"] = ft2;

		Upsert(ns, item);
		Commit(ns);
	}
	QueryResults SimpleSelect(string word) {
		Query qr = Query("nm1").Where("ft3", CondEq, word);
		QueryResults res;
		qr.AddFunction("ft3 = highlight(!,!)");
		reindexer->Select(qr, res);

		return res;
	}

	void Delete(int id) {
		Item item = NewItem("nm1");
		item["id"] = id;

		this->reindexer->Delete("nm1", item);
	}
	QueryResults SimpleCompositeSelect(string word) {
		Query qr = Query("nm1").Where("ft3", CondEq, word);
		QueryResults res;
		Query mqr = Query("nm2").Where("ft3", CondEq, word);
		mqr.AddFunction("ft1 = snippet(<b>,\"\"</b>,3,2,,d)");

		qr.mergeQueries_.push_back(mqr);
		qr.AddFunction("ft3 = highlight(<b>,</b>)");
		reindexer->Select(qr, res);

		return res;
	}

	FTApi() {}

private:
	struct Data {
		std::string ft1;
		std::string ft2;
	};
	int counter_ = 0;
};
