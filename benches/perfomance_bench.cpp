#include "bench.hpp"

static void PerfomanceInsertString(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();

        auto file = util::MakePtr<mem::File>("perf.ddb");
        auto database = db::Database(file, db::OpenMode::kWrite);

        auto name = ts::NewClass<ts::StringClass>("name");
        database.AddClass(name);

        state.ResumeTiming();

        for (int64_t i = 0; i < state.range(0); ++i) {
            database.AddNode(ts::New<ts::String>(name, "test name"));
        }
    }
}

static void PerfomanceInsertPrimitive(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();

        auto file = util::MakePtr<mem::File>("perf.ddb");
        auto database = db::Database(file, db::OpenMode::kWrite);

        auto age = ts::NewClass<ts::PrimitiveClass<int>>("age");
        database.AddClass(age);

        state.ResumeTiming();

        for (int64_t i = 0; i < state.range(0); ++i) {
            database.AddNode(ts::New<ts::Primitive<int>>(age, 100));
        }
    }
}

static void PerfomanceRemoveByValue(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto file = util::MakePtr<mem::File>("perf.ddb");
        auto database = db::Database(file, db::OpenMode::kWrite);

        auto name = ts::NewClass<ts::StringClass>("name");
        auto age = ts::NewClass<ts::PrimitiveClass<int>>("age");

        database.AddClass(name);
        database.AddClass(age);

        int64_t size = state.range(0);
        
        for (int64_t i = 0; i < size; ++i) {
            database.AddNode(ts::New<ts::Primitive<int>>(age, 100'000));
            database.AddNode(ts::New<ts::String>(name, "test name"));
        }

        state.ResumeTiming();

        for (int64_t i = 0; i < size; ++i) {
            database.RemoveNodesIf(age, [i](auto it) { return it->Id() == ID(i); });
        }
    }
}

static void PerfomanceRemoveByVariable(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto file = util::MakePtr<mem::File>("perf.ddb");
        auto database = db::Database(file, db::OpenMode::kWrite);

        auto name = ts::NewClass<ts::StringClass>("name");
        auto age = ts::NewClass<ts::PrimitiveClass<int>>("age");

        database.AddClass(name);
        database.AddClass(age);

        int64_t size = state.range(0);
        for (int64_t i = 0; i < size; ++i) {
            database.AddNode(ts::New<ts::Primitive<int>>(age, 100'000));
            database.AddNode(ts::New<ts::String>(name, "test name"));
        }

        state.ResumeTiming();

        for (int64_t i = 0; i < size; ++i) {
            database.RemoveNodesIf(name, [i](auto it) { return it->Id() == ID(i); });
        }
    }
}

static void PerfomanceMatch(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        auto point = ts::NewClass<ts::PrimitiveClass<int>>("point");
        auto edge = ts::NewClass<ts::RelationClass>("edge", point, point);

        auto database =
            util::MakePtr<db::Database>(util::MakePtr<mem::File>("perf.data"), db::OpenMode::kWrite);
        database->AddClass(point);
        database->AddClass(edge);

        int size = state.range(0);
        for (int i = 0; i < size; ++i) {
            database->AddNode(ts::New<ts::Primitive<int>>(point, i));
        }
        for (int i = 1; i < size; ++i) {
            database->AddNode(ts::New<ts::Relation>(edge, ID(0), ID(i)));
            database->AddNode(ts::New<ts::Relation>(edge, ID(i), ID(0)));
        }
        for (int i = 1; i < size; ++i) {
            for (int j = 1; j < i; ++j) {
                auto star = util::MakePtr<db::Pattern>(point);
                star->AddRelation(edge, [i](db::Node a, db::Node b) {
                    return a.Data<ts::Primitive<int>>()->Value() == 0 &&
                        b.Data<ts::Primitive<int>>()->Value() == i;
                });
                star->AddRelation(edge, [j](db::Node a, db::Node b) {
                    return a.Data<ts::Primitive<int>>()->Value() == 0 &&
                        b.Data<ts::Primitive<int>>()->Value() == j;
                });
                std::vector<ts::Struct::Ptr> result;
                state.ResumeTiming();
                database->PatternMatch(star, std::back_inserter(result));
                state.PauseTiming();
            }
        }

        state.ResumeTiming();
    }
}

BENCHMARK(PerfomanceInsertString)->Arg(10'000);
BENCHMARK(PerfomanceInsertPrimitive)->Arg(10'000);
BENCHMARK(PerfomanceRemoveByValue)->Arg(10'000);
BENCHMARK(PerfomanceRemoveByVariable)->Arg(10'000);
BENCHMARK(PerfomanceMatch)->Arg(100);

BENCHMARK_MAIN();
