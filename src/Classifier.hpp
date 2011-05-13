#ifndef CLASSIFIER_INCLUDE_GUARD
#define CLASSIFIER_INCLUDE_GUARD

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

enum classifier_mode_t {
  PREPARE_MODE,
  TRAIN_MODE,
  TOKENIZE_MODE,
  EVALUATE_MODE
};

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
           classifier_mode_t mode,
           std::vector<std::string> const &property_names,
           int precontext,
           int postcontext,
           bool *features_mask,
           std::vector< std::vector< std::pair<int,int> > > combined_features,
           std::ostream *qa_stream_p = NULL,
           std::istream *annot_stream_p = NULL):
              tbb::filter(tbb::filter::serial_in_order),
              m_mode(mode),
              m_property_names(property_names),
              m_precontext(precontext),
              m_postcontext(postcontext),
              m_window_size(precontext + 1 + postcontext),
              m_features_mask(features_mask),
              m_combined_features(combined_features),
              m_qa_stream_p(qa_stream_p),
              m_annot_stream_p(annot_stream_p),
              m_n_events_registered(0)
    {
        m_window = new token_t[m_window_size];
        if (m_mode == TRAIN_MODE) {
          m_model.begin_add_event();
        }
        reset();
    }

    void setup(std::string processed_filename) {
        m_processed_filename = processed_filename;
        reset();
    }

    void reset() {
        m_first_chunk = true;
        for (int i = 0; i < m_window_size; i++) {
          m_window[i].text = "";
          m_window[i].decision_flags = NO_FLAG;
        }
        m_center_token = 0;
    }

    void load_model(std::string const &model_path) {
      m_model.load(model_path);
    }

    void train_model(training_parameters_t const &training_parameters,
                     std::string const &model_path,
                     bool save_as_binary = false) {

      m_model.end_add_event(training_parameters.event_cutoff);
      if (m_n_events_registered > 0) {
        m_model.train(training_parameters.n_iterations,
                      training_parameters.method_name,
                      training_parameters.smoothing_coefficient,
                      training_parameters.convergence_tolerance);
      }
      m_model.save(model_path, save_as_binary);
    }

    ~Classifier() {
        delete[] m_window;
    }

    void process_tokens(std::vector<token_t> &tokens, chunk_t *out_chunk_p);
    void process_center_token(chunk_t *out_chunk_p);
    void align_chunk_with_solution(chunk_t *in_chunk_p);
    virtual void* operator()(void *input_p);

private:
    bool consume_whitespace();
    void report_alignment_warning(std::string occurence_type,
                                  std::string prefix, std::string suffix,
                                  std::string advice);
private:
    // Configuration
    classifier_mode_t m_mode;
    std::vector<std::string> m_property_names;
    int m_precontext;
    int m_postcontext;
    int m_window_size;
    bool *m_features_mask;
    std::vector< std::vector< std::pair<int,int> > > m_combined_features;
    std::ostream *m_qa_stream_p;
    std::istream *m_annot_stream_p;
    std::string m_processed_filename;

    // State
    uint32_t m_annot_char;
    bool m_first_chunk;
    token_t *m_window;
    int m_center_token;
    maxent::MaxentModel m_model;
    int m_n_events_registered;
};

}

#endif
