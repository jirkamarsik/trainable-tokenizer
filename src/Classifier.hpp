#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include "tbb/pipeline.h"
#include <boost/cstdint.hpp>
typedef boost::uint32_t uint32_t;
#include <maxentmodel.hpp>

#include <token_t.hpp>

namespace trtok {

struct training_parameters_t {
  size_t event_cutoff;
  size_t n_iterations;
  std::string method_name;
  double smoothing_coefficient;
  double convergence_tolerance;

  training_parameters_t():
    event_cutoff(1),
    n_iterations(15),
    method_name("lbfgs"),
    smoothing_coefficient(0.0),
    convergence_tolerance(1e-05)
  {}
};

class Classifier: public tbb::filter {
public:
    Classifier(
           std::string mode,
           std::vector<std::string> const &property_names,
           int precontext,
           int postcontext,
           bool *features_map,
           std::vector< std::vector< std::pair<int,int> > > combined_features,
           bool print_questions,
           std::istream *annot_stream_p = NULL):
              tbb::filter(tbb::filter::serial_in_order),
              m_mode(mode),
              m_property_names(property_names),
              m_precontext(precontext),
              m_postcontext(postcontext),
              m_window_size(precontext + 1 + postcontext),
              m_features_map(features_map),
              m_combined_features(combined_features),
              m_print_questions(print_questions),
              m_annot_stream_p(annot_stream_p)
        {
            m_window = new token_t[m_window_size];
            m_model.begin_add_event();
            reset();
        }

    void reset() {
        first_chunk = true;
        for (int i = 0; i < m_window_size; i++) {
          m_window[i].text = "";
        }
        m_center_token = 0;
    }

    void train_model(training_parameters_t const &training_parameters,
                     std::string const &model_path,
                     bool save_as_binary = false) {
      m_model.end_add_event(training_parameters.event_cutoff);
      m_model.train(training_parameters.n_iterations,
                    training_parameters.method_name,
                    training_parameters.smoothing_coefficient,
                    training_parameters.convergence_tolerance);
      m_model.save(model_path, save_as_binary);
    }

    ~Classifier() {
        delete[] m_window;
    }

    virtual void* operator()(void *input_p);
private:
    bool consume_whitespace();
    void report_alignment_warning(std::string occurence_type,
    std::string prefix, std::string suffix, std::string advice);
private:
    // Configuration
    std::string m_mode;
    std::vector<std::string> m_property_names;
    int m_precontext;
    int m_postcontext;
    int m_window_size;
    bool *m_features_map;
    std::vector< std::vector< std::pair<int,int> > > m_combined_features;
    bool m_print_questions;
    std::istream *m_annot_stream_p;

    // State
    uint32_t annot_char;
    bool first_chunk;
    token_t *m_window;
    int m_center_token;
    maxent::MaxentModel m_model;
};

}
