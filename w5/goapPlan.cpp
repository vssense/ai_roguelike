#include "goapPlanner.h"
#include <algorithm>
#include <cfloat>

struct PlanNode
{
  goap::WorldState worldState;
  goap::WorldState prevState;

  float g = 0;
  float h = 0;

  size_t actionId;
};

static float heuristic(const goap::WorldState &from, const goap::WorldState &to)
{
  float cost = 0;
  for (size_t i = 0; i < to.size(); ++i)
    if (to[i] >= 0) // we care about it
      cost += float(abs(to[i] - from[i]));
  return cost;
}

static void reconstruct_plan(PlanNode &goal_node, const std::vector<PlanNode> &closed, std::vector<goap::PlanStep> &plan)
{
  PlanNode &curNode = goal_node;
  while (curNode.actionId != size_t(-1))
  {
    plan.push_back({curNode.actionId, curNode.worldState});
    auto itf = std::find_if(closed.begin(), closed.end(), [&](const PlanNode &n) { return n.worldState == curNode.prevState; });
    curNode = *itf;
  }
  std::reverse(plan.begin(), plan.end());
}

namespace goap
{
static float search_ida_star(const Planner &planner, const WorldState &from, float g, float bound, const WorldState &to, std::vector<PlanStep> &plan)
{
  float f = g + heuristic(from, to);
  if (f > bound)
    return f;
  
  if (heuristic(from, to) == 0)
    return -f;
  
  auto transitions = find_valid_state_transitions(planner, from);
  float min = FLT_MAX;
  for (auto actId : transitions)
  {
    auto next = apply_action(planner, actId, from);
    auto it = std::find_if(plan.begin(), plan.end(), [&next](const auto& step) {
      return step.worldState == next;
    });

    if (it != plan.end())
      continue;
    
    plan.push_back({actId, next});
    float g_next = g + get_action_cost(planner, actId);
    float search_res = search_ida_star(planner, next, g_next, bound, to, plan);
    if (search_res < 0.f)
      return search_res;
    
    if (search_res < min)
      min = search_res;

    plan.pop_back();
  }

  return min;
}
}

float goap::make_plan_ida_star(const Planner &planner, const WorldState &from, const WorldState &to, std::vector<PlanStep> &plan)
{
  float bound = heuristic(from, to);
  plan.clear();

  while (true)
  {
    float result = search_ida_star(planner, from, 0.f, bound, to, plan);
    if (result < 0.f)
      return -result;

    if (result == FLT_MAX)
      break;
    
    bound = result;
  }

  return 0.f;
}

float goap::make_plan(const Planner &planner, const WorldState &from, const WorldState &to, std::vector<PlanStep> &plan)
{
  std::vector<PlanNode> openList = {PlanNode{from, from, 0, heuristic(from, to), size_t(-1)}};
  std::vector<PlanNode> closedList = {};
  while (!openList.empty())
  {
    auto minIt = openList.begin();
    float minF = minIt->g + minIt->h;
    for (auto it = openList.begin(); it != openList.end(); ++it)
      if (it->g + it->h < minF)
      {
        minF = it->g + it->h;
        minIt = it;
      }
    PlanNode cur = *minIt;
    openList.erase(minIt);
    if (heuristic(cur.worldState, to) == 0) // we've reached our goal
    {
      reconstruct_plan(cur, closedList, plan);
      return minF;
    }
    closedList.push_back(cur);
    std::vector<size_t> transitions = find_valid_state_transitions(planner, cur.worldState);
    //const bool firstIter = openList.empty();
    //printf("------------\n");
    for (size_t actId : transitions)
    {
      //printf("valid action: %s\n", planner.actions[actId].name.c_str());
      WorldState st = apply_action(planner, actId, cur.worldState);
      const float score = cur.g + get_action_cost(planner, actId);
      auto openIt = std::find_if(openList.begin(), openList.end(), [&](const PlanNode &n) { return st == n.worldState; });
      auto closeIt = std::find_if(closedList.begin(), closedList.end(), [&](const PlanNode &n) { return st == n.worldState; });
      if (openIt != openList.end() && score < openIt->g)
      {
        openIt->g = score;
        openIt->prevState = cur.worldState;
      }
      if (closeIt != closedList.end() && score < closeIt->g)
      {
        closeIt->g = score;
        closeIt->prevState = cur.worldState;
      }
      if (closeIt == closedList.end() && openIt == openList.end())
        openList.push_back({st, cur.worldState, score, heuristic(st, to), actId});
    }
  }
  return 0.f;
}

void goap::print_plan(const Planner &planner, const WorldState &init, const std::vector<PlanStep> &plan)
{
  printf("%15s: ", "");
  std::vector<int> dlen;
  for (size_t i = 0; i < planner.wdesc.size(); ++i)
  {
    // print names by searching
    for (auto it : planner.wdesc)
    {
      if (it.second == i)
      {
        printf("|%s|", it.first.c_str());
        dlen.push_back(int(it.first.size()));
        break;
      }
    }
  }
  printf("\n");
  printf("%15s: ", "");
  for (size_t i = 0; i < init.size(); ++i)
    printf("|%*d|", dlen[i], init[i]);
  printf("\n");
  for (const PlanStep &step : plan)
  {
    printf("%15s: ", planner.actions[step.action].name.c_str());
    for (size_t i = 0; i < step.worldState.size(); ++i)
      printf("|%*d|", dlen[i], step.worldState[i]);
    printf("\n");
  }
}

