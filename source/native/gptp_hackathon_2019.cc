//  This file is part of gptp_hackathon_2019
//  Copyright (C) Emily Dolson, 2019.
//  Released under MIT license; see LICENSE

#include <iostream>

#include "base/vector.h"
#include "config/command_line.h"
#include "config/ArgManager.h"

// This is the main function for the NATIVE version of gptp_hackathon_2019.
#include "Evolve/World.h"
#include "hardware/AvidaGP.h"

EMP_BUILD_CONFIG( EcologyConfig,
  GROUP(MAIN, "Global settings"),
  VALUE(MODE, int, 0, "0 = run experiment, 1 = Just run ecology stats so we can time it."),
  VALUE(SEED, int, 0, "Random number seed (0 for based on time)"),
  VALUE(START_POP_SIZE, uint32_t, 1, "Number of organisms to seed population with."),
  VALUE(POP_SIZE, uint32_t, 1000, "Number of organisms in the popoulation."),
  VALUE(MAX_GENS, uint32_t, 2000, "How many generations should we process?"),
  VALUE(MUT_RATE, double, .005, "Probability of each site being mutated. For real-valued problems, the standard deviation of of the distribution from which mutations are pulled. For program, the probability of instruction mutation to a different one."),
  VALUE(PROBLEM, uint32_t, 0, "Which problem to use? 0 = NK, 1 = Program synthesis, 2 = Real-valued, 3 = Sorting network, 4 = Logic-9"),

  GROUP(NK, "Settings for NK landscape"),
  VALUE(K, uint32_t, 10, "Level of epistasis in the NK model"),
  VALUE(N, uint32_t, 200, "Number of bits in each organisms (must be > K)"),

  GROUP(REAL_VALUED, "Settings for real-valued optimzation problems"),
  VALUE(FUNCTION_NUMBER, uint32_t, 0, "Problem to use"),
  VALUE(DIMS, uint32_t, 200, "Number of dimensions in orgs"),

  GROUP(SORTING_NETWORKS, "Setting for sorting network problems"),
  VALUE(NUM_BITS, size_t, 16, "Length of input to sorting network."),
  VALUE(MAX_NETWORK_SIZE, size_t, 128, "Maximum size of a sorting network."),
  VALUE(MIN_NETWORK_SIZE, size_t, 1, "Minimum size of a sorting network."),
  VALUE(PER_INDEX_SUB, double, 0.001, "."),
  VALUE(PER_PAIR_DUP, double, 0.0005, "."),
  VALUE(PER_PAIR_INS, double, 0.0005, "."),
  VALUE(PER_PAIR_DEL, double, 0.001, "."),
  VALUE(PER_PAIR_SWAP, double, 0.001, "."),

  GROUP(PROGRAM_SYNTHESIS, "Settings for evolving code"),
  VALUE(GENOME_SIZE, uint32_t, 100, "Starting length of genome"),
  VALUE(MAX_GENOME_SIZE, uint32_t, 1000, "Maximum length of genome"),
  VALUE(EVAL_TIME, uint32_t, 100, "How long to run program for"),
  VALUE(INS_MUT_RATE, double, 0.005, "Probability of insertion mutation per instruction"),  
  VALUE(DEL_MUT_RATE, double, 0.005, "Probability of deletion mutation per instruction"),  
  VALUE(ARG_SUB_RATE, double, 0.005, "Probability argument substituion per instruction"),  

  GROUP(TESTCASES, "Settings for problems that use testcases"),
  VALUE(TRAINSET_FILE, std::string, "testcases/count-odds.csv", "Which set of testcases should we use for training?"),  
  VALUE(TESTSET_FILE, std::string, "testcases/count-odds.csv", "Which set of testcases should we use for evaluation?"),
  VALUE(N_TEST_CASES, uint32_t, 200, "How many test cases to use"),  

  GROUP(SELECTION_METHODS, "Settings related to selection"),
  VALUE(SELECTION, uint32_t, 0, "Selection method. 0 = Tournament, 1 = fitness sharing, 2 = lexicase, 3 = Eco-EA, 4 = Random"),
  VALUE(TOURNAMENT_SIZE, int, 2, "For tournament selection, number of individuals to include in tournament"),
  VALUE(SHARING_ALPHA, double, 1, "Alpha for fitness sharing (controls shape of sharing function)"),
  VALUE(SHARING_THRESHOLD, double, 2, "For fitness sharing - how similar do two individuals have to be to compete?"),
  VALUE(RESOURCE_SELECT_RES_AMOUNT, double, 50.0, "Initial resource amount (for all resources)"),
  VALUE(RESOURCE_SELECT_RES_INFLOW, double, 50.0, "Resource in-flow (amount)"),
  VALUE(RESOURCE_SELECT_RES_OUTFLOW, double, 0.05, "Resource out-flow (rate)"),
  VALUE(RESOURCE_SELECT_FRAC, double, 0.0025, "Fraction of resource consumed."),
  VALUE(RESOURCE_SELECT_MAX_BONUS, double, 5.0, "What's the max bonus someone can get for consuming a resource?"),
  VALUE(RESOURCE_SELECT_COST, double, 0.0, "Cost of using a resource?"),
  VALUE(RESOURCE_SELECT_NICHE_WIDTH, double, 0.0, "Score necessary to count as attempting to use a resource"),
  VALUE(LEXICASE_EPSILON, double, 0.0, "To use epsilon-lexicase, set this to an epsilon value greater than 0"),

  GROUP(OPEN_ENDED_EVOLUTION, "Settings related to tracking MODES metrics"),
  VALUE(MODES_RESOLUTION, int, 1, "How often should MODES metrics be calculated?"),
  VALUE(FILTER_LENGTH, int, 1000, "How many generations should we use for the persistence filter?"),

  GROUP(DATA_RESOLUTION, "How often should data get printed?"),
  VALUE(FAST_DATA_RES, int, 10, "How often should easy to calculate metrics (e.g. fitness) be calculated?"),
  VALUE(ECOLOGY_DATA_RES, int, 100, "How often should ecological interactions (expensive) be calculated?")
);

struct test_case {
  emp::vector<double> initial_conditions;
  emp::vector<double> ages;
};

emp::vector<test_case> LoadTestcases(std::string filename) {
    std::ifstream infile(filename);
    std::string line;

    emp::vector<test_case> cases;

    if (!infile.is_open()){
        std::cout << "ERROR: " << filename << " did not open correctly" << std::endl;
        return cases;
    }

    // Ignore header
    getline(infile, line);

    test_case t;
    while ( getline (infile,line)) {
        emp::vector<std::string> split_line = emp::slice(line, ',');
        if (split_line[0] == "0") {
          cases.push_back(t);
          t = test_case();
        }

        for (size_t i = 0; i < 5; i++) {
            t.initial_conditions.push_back(std::atoi(split_line[i].c_str()));
        }

        t.ages.push_back(std::atoi(split_line[33].c_str()));

    }
    infile.close();
    return cases;
}

int main(int argc, char* argv[])
{
  using fit_fun_t = std::function<double(emp::AvidaGP &)>;
  EcologyConfig config;

  auto args = emp::cl::ArgManager(argc, argv);
  if (args.ProcessConfigOptions(config, std::cout, "EcologyConfig.cfg", "Ecology-macros.h") == false) exit(0);
  if (args.TestUnknown() == false) exit(0);  // If there are leftover args, throw an error.

  emp::World<emp::AvidaGP> w;
  emp::vector<test_case> cases = LoadTestcases("MESA_sequences.csv");
  emp::vector<fit_fun_t> fit_set;
  for (test_case t : cases) {
    fit_set.push_back([t](emp::AvidaGP & o){
      double error = 0;
      for (int i = 0; i < t.initial_conditions.size(); i++) {
        o.SetInput(i, t.initial_conditions[i]);
      }

      for (int step = 0; step < t.ages.size(); step++) {
        o.SingleProcess();
        error += pow(o.GetOutput(0) - t.ages[step],2);
      }
      return -1 * error;
    });
  }

  w.SetFitFun([fit_set](emp::AvidaGP & o){
    double summed_error = 0;
    for (auto fit_fun : fit_set) {
      summed_error += fit_fun(o);
    }
    return summed_error;
  });

  auto & fit_file = w.SetupFitnessFile();
  fit_file.SetTimingRepeat(5);

  for (size_t i = 0; i < config.POP_SIZE(); i++) {
      emp::vector<double> vec;
      emp::AvidaGP cpu;
      cpu.PushRandom(w.GetRandom(), config.GENOME_SIZE());
      w.Inject(cpu.GetGenome());
  }

  w.SetMutFun([&config](emp::AvidaGP & org, emp::Random & random){
    uint32_t num_muts = random.GetUInt(config.MUT_RATE());  // 0 to 3 mutations.
    for (uint32_t m = 0; m < num_muts; m++) {
        const uint32_t pos = random.GetUInt(config.GENOME_SIZE());
        org.RandomizeInst(pos, random);
    }
    return num_muts;
  });

  w.SetAutoMutate();
  w.SetPopStruct_Mixed(true);

  for (int ud = 0; ud < 1000; ud++) {
    std::cout << ud << std::endl;
    emp::LexicaseSelect(w, fit_set, config.POP_SIZE());
    w.Update();
  }

}
