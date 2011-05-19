#!/usr/bin/python3

import fileinput

events = 0
eff_success = 0

seg_tp = 0
seg_fp = 0
seg_tn = 0
seg_fn = 0

# A positive results in tokenization is understood as the occasion
# when the decision point between the tokens becomes a token boundary.
tok_tp = 0
tok_fp = 0
tok_tn = 0
tok_fn = 0

for line in fileinput.input():
  line_parts = line.split('|', 4)

  file_path = line_parts[0]
  predicted_outcome = line_parts[1]
  true_outcome = line_parts[2]
  features_line = line_parts[3].strip()

  events = events + 1

  if predicted_outcome == true_outcome:
    eff_success = eff_success + 1

  if '0:%MAY_BREAK_SENTENCE' in features_line:
    if predicted_outcome == 'BREAK_SENTENCE':
      if true_outcome == 'BREAK_SENTENCE':
        seg_tp = seg_tp + 1
      else:
        seg_fp = seg_fp + 1
    else:
      if true_outcome == 'BREAK_SENTENCE':
        seg_fn = seg_fn + 1
      else:
        seg_tn = seg_tn + 1

  if '0:%MAY_SPLIT' in features_line\
   or '0:%MAY_JOIN' in features_line:
    if predicted_outcome == 'JOIN':
      if true_outcome == 'JOIN':
        tok_tn = tok_tn + 1
      else:
        tok_fn = tok_fn + 1
    else:
      if true_outcome == 'JOIN':
        tok_fp = tok_fp + 1
      else:
        tok_tp = tok_tp + 1


eff_tot = events
eff_acc = eff_success / float(eff_tot) if eff_tot > 0 else 'no data'

 
seg_tot = seg_tp + seg_tn + seg_fp + seg_fn
seg_acc = (seg_tp + seg_tn) / float(seg_tot) \
            if seg_tot > 0 else 'no data'

seg_true_t = seg_tp + seg_fp
seg_prec = seg_tp / float(seg_true_t) if seg_true_t > 0 else 'no data'

seg_pred_t = seg_tp + seg_fn
seg_rec = seg_tp / float(seg_pred_t) if seg_pred_t > 0 else 'no data'

seg_fm = 2 * (seg_prec * seg_rec) / float(seg_prec + seg_rec) \
          if seg_prec != 'no data' and seg_rec != 'no data' \
          and seg_prec + seg_rec > 0  else 'no data'


tok_tot = tok_tp + tok_tn + tok_fp + tok_fn
tok_acc = (tok_tp + tok_tn) / float(tok_tot) \
            if tok_tot > 0 else 'no data'

tok_true_t = tok_tp + tok_fp
tok_prec = tok_tp / float(tok_true_t) if tok_true_t > 0 else 'no data'

tok_pred_t = tok_tp + tok_fn
tok_rec = tok_tp / float(tok_pred_t) if tok_pred_t > 0 else 'no data'

tok_fm = 2 * (tok_prec * tok_rec) / float(tok_prec + tok_rec) \
          if tok_prec != 'no data' and tok_rec != 'no data' \
          and tok_prec + tok_rec > 0  else 'no data'


print('Number of events:', eff_tot)
print('')
print('Effective accuracy:', eff_acc)
print('')
print('Segmentation accuracy:', seg_acc)
print('Segmentation precision:', seg_prec)
print('Segmentation recall:', seg_rec)
print('Segmentation F-measure:', seg_fm)
print('')
print('Tokenization accuracy:', tok_acc)
print('Tokenization precision:', tok_prec)
print('Tokenization recall:', tok_rec)
print('Tokenization F-measure:', tok_fm)
