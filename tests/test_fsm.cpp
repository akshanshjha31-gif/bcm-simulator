/**
 * @file    test_fsm.cpp
 * @brief   Unit tests for the shared table-driven FSM engine.
 */
#include "catch2/catch.hpp"
#include "fsm.h"

using bcm::services::Fsm;
using bcm::services::FsmTransition;

namespace {

enum class S : uint8_t { A, B, C };
enum class E : uint8_t { Go, Back, Reset, Blocked };

struct Ctx {
    bool allow    = true;
    int  actions  = 0;
};

bool guard_allow(const Ctx& c) { return c.allow; }
void bump(Ctx& c) { c.actions++; }

typedef FsmTransition<S, E, Ctx> Row;

const Row kTable[] = {
    { S::A, false, E::Go,      S::B, 0,           bump },
    { S::B, false, E::Go,      S::C, 0,           0    },
    { S::B, false, E::Back,    S::A, 0,           0    },
    { S::C, false, E::Blocked, S::A, guard_allow, bump },
    { S::A, true,  E::Reset,   S::A, 0,           0    },
};
const uint8_t kRows = 5U;

}  // namespace

TEST_CASE("A new FSM sits in its initial state", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(kTable, kRows, S::A);
    REQUIRE(fsm.state() == S::A);
    REQUIRE(fsm.transition_count() == 0U);
}

TEST_CASE("A matching event transitions and runs its action", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(kTable, kRows, S::A);
    Ctx ctx;

    REQUIRE(fsm.dispatch(E::Go, ctx));
    REQUIRE(fsm.state() == S::B);
    REQUIRE(ctx.actions == 1);
    REQUIRE(fsm.transition_count() == 1U);
}

TEST_CASE("An event with no rule for the current state is ignored", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(kTable, kRows, S::A);
    Ctx ctx;

    REQUIRE_FALSE(fsm.dispatch(E::Back, ctx));   // Back is only valid from B
    REQUIRE(fsm.state() == S::A);
    REQUIRE(ctx.actions == 0);
}

TEST_CASE("A guard that refuses blocks the transition and its action", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(kTable, kRows, S::C);
    Ctx ctx;
    ctx.allow = false;

    REQUIRE_FALSE(fsm.dispatch(E::Blocked, ctx));
    REQUIRE(fsm.state() == S::C);
    REQUIRE(ctx.actions == 0);           // action must NOT run
    REQUIRE(fsm.blocked_count() == 1U);
    REQUIRE(fsm.transition_count() == 0U);
}

TEST_CASE("A guard that permits lets the transition through", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(kTable, kRows, S::C);
    Ctx ctx;
    ctx.allow = true;

    REQUIRE(fsm.dispatch(E::Blocked, ctx));
    REQUIRE(fsm.state() == S::A);
    REQUIRE(ctx.actions == 1);
    REQUIRE(fsm.blocked_count() == 0U);
}

TEST_CASE("A from_any row matches regardless of state", "[fsm]")
{
    Ctx ctx;
    for (S start : { S::A, S::B, S::C }) {
        Fsm<S, E, Ctx> fsm(kTable, kRows, start);
        REQUIRE(fsm.dispatch(E::Reset, ctx));
        REQUIRE(fsm.state() == S::A);
    }
}

TEST_CASE("A refusing guard is not bypassed by a later matching row", "[fsm]")
{
    /* If a blocked guard fell through to the next row, a safety rule could be
     * silently routed around by adding a catch-all below it. */
    const Row table[] = {
        { S::A, false, E::Go, S::B, guard_allow, 0 },
        { S::A, true,  E::Go, S::C, 0,           0 },   // catch-all below
    };
    Fsm<S, E, Ctx> fsm(table, 2U, S::A);
    Ctx ctx;
    ctx.allow = false;

    REQUIRE_FALSE(fsm.dispatch(E::Go, ctx));
    REQUIRE(fsm.state() == S::A);       // must NOT have fallen through to C
}

TEST_CASE("set_state bypasses guards, for reset paths only", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(kTable, kRows, S::A);
    fsm.set_state(S::C);
    REQUIRE(fsm.state() == S::C);
    REQUIRE(fsm.transition_count() == 0U);   // not counted as a transition
}

TEST_CASE("An empty table never transitions", "[fsm]")
{
    Fsm<S, E, Ctx> fsm(0, 0U, S::A);
    Ctx ctx;
    REQUIRE_FALSE(fsm.dispatch(E::Go, ctx));
    REQUIRE(fsm.state() == S::A);
}
