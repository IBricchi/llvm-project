#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ReplayInlineAdvisor.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/Utils/ImportedFunctionsInliningStatistics.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace llvm;
#define DEBUG_TYPE "inline"

// declarations
void getAdvice(InlineAdvisor &advisor, OptimizationRemarkEmitter &ORE,
               CallBase &CB, bool MandatoryOnly,
               std::unique_ptr<InlineAdvice> &defaultAdvice, std::string &ctx);
void parse_advice_file(std::string &filename);

// globals
static enum {
  NONE,
  DEFAULT,
  OVERRIDE,
} advice_mode = NONE;

static std::unordered_map<std::string, bool> advice_map;

static std::vector<std::string> seen_inline_locations;
static std::vector<std::string> decisions_taken;

// lifetime helper
struct LifetimeDetector {
  LifetimeDetector();
  ~LifetimeDetector();
} lifetime_detector;

LifetimeDetector::LifetimeDetector() {
  // std::cout << "handle start of pass" << std::endl;
}
LifetimeDetector::~LifetimeDetector() {
  std::cout << "locations seen: " << std::endl;
  for (std::string &loc : seen_inline_locations) {
    std::cout << loc << std::endl;
  }
  std::cout << "decisions made: " << std::endl;
  for (std::string &decision : decisions_taken) {
    std::cout << decision << std::endl;
  }
  if (advice_mode == OVERRIDE) {
    std::cout << "advice map: " << std::endl;
    for (auto &pair : advice_map) {
      std::cout << pair.first << " -> " << pair.second << std::endl;
    }
  }
}

// external interface
extern "C" void dynamic_getAdvice(InlineAdvisor *advisor,
                                  OptimizationRemarkEmitter *ORE, CallBase *CB,
                                  bool *MandatoryOnly,
                                  std::unique_ptr<InlineAdvice> *defaultAdvice,
                                  std::string *ctx) {
  getAdvice(*advisor, *ORE, *CB, *MandatoryOnly, *defaultAdvice, *ctx);
}

// helpers
static uint64_t pairing_function(uint64_t a, uint64_t b) {
  return (a + b) * (a + b + 1) / 2 + b;
}

std::string getLocString(DebugLoc loc, bool show_inlining) {
  std::string OS{};
  // TODO! migrate this to llvm
  if (loc.get() == nullptr) {
    exit(1);
  }
  // end migrate
  OS += loc->getFilename();
  OS += ":";
  OS += std::to_string(loc.getLine());
  if (loc.getCol() != 0) {
    OS += ":";
    OS += std::to_string(loc.getCol());
  }

  if (show_inlining) {
    if (DebugLoc InlinedAtDL = loc->getInlinedAt()) {
      OS += "@[";
      OS += getLocString(InlinedAtDL, true);
      OS += "]";
    }
  }

  return OS;
}

// main function
void getAdvice(InlineAdvisor &advisor, OptimizationRemarkEmitter &ORE,
               CallBase &CB, bool MandatoryOnly,
               std::unique_ptr<InlineAdvice> &defaultAdvice, std::string &ctx) {

  if (advice_mode == NONE) {
    advice_mode = ctx.empty() ? DEFAULT : OVERRIDE;
    if (advice_mode == OVERRIDE) {
      std::cout << "Override mode" << std::endl;
      parse_advice_file(ctx);
    }
  }

  StringRef Callee = CB.getCalledFunction()->getName();

  std::string CallLocation{};

  DebugLoc loc = defaultAdvice->getOriginalCallSiteDebugLoc();
  seen_inline_locations.push_back(getLocString(loc, false));
  CallLocation = getLocString(loc, true);

  // change any decision to a false decision
  if (advice_mode == OVERRIDE) {
    if (advice_map.find(CallLocation) != advice_map.end()) {
      defaultAdvice->recordUnattemptedInlining();
      defaultAdvice = std::make_unique<InlineAdvice>(&advisor, CB, ORE, advice_map[CallLocation]);
    }
  }

  // print decision
  std::stringstream ss{};
  ss << Callee.str() << " @ " << CallLocation << " : "
     << (defaultAdvice->isInliningRecommended() ? "inline" : "no-inline");
  decisions_taken.push_back(ss.str());
}

// parse advice file
void parse_advice_file(std::string &filename) {
  std::ifstream file(filename);

  enum {
    NODE_LIST_HEADER,
    NODE_LIST,
    DECISION_LIST,
    ERROR,
  } state = NODE_LIST_HEADER;

  std::string line;
  while (std::getline(file, line)) {
    switch (state) {
    case NODE_LIST_HEADER:
      {
        if (line == "locations seen: ") {
          state = NODE_LIST;
        } else {
          state = ERROR;
        }
      }
      break;
    case NODE_LIST:
      {
        if (line == "decisions made: ") {
          state = DECISION_LIST;
        }
      }
      break;
    case DECISION_LIST:
      {
        std::istringstream iss(line);
        std::string tmp;
        std::string callee;
        std::string location;
        std::string decision;
        if (!(iss >> callee >> tmp >> location >> tmp >> decision)) {
          state = ERROR;
        } else {
          advice_map.insert({location, decision == "inline"});
        }
      }
      break;
    }
  }

  if (state == ERROR) {
    std::cerr << "error parsing advice file" << std::endl;
    exit(1);
  }
}