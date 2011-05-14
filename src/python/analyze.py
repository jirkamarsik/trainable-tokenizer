#!/usr/bin/python3

import fileinput

events = list()

for line in fileinput.input():
  line_parts = line.split('|', 4)

  file_path = line_parts[0]
  predicted_outcome = line_parts[1]
  true_outcome = line_parts[2]
  features_line = line_parts[3].strip()

  features = list()
  for feature_string in features_line.split():
    # a combined feature
    if feature_string[0] == '(':
      features.append( (feature_string, 1.0) )
    # a valued feature (length)
    elif '=' in feature_string:
      feature, eq, value = feature_string.partition('=')
      # WARNING: In the case of %Word, the value isn't the expected float (1.0)
      #          as is the case with every other feature, bit instead the value
      #          is equal to the text of the token. This might be unwieldy.
      features.append( (feature, value) )
    # a simple binary feature
    else:
      features.append( (feature_string, 1.0) )

  events.append(dict(file_path=file_path, predicted_outcome=predicted_outcome,
                     true_outcome=true_outcome, features=features))


eff_success = list()

seg_tp = list()
seg_fp = list()
seg_tn = list()
seg_fn = list()

# A positive results in tokenization is understood as the occasion
# when the decision point between the tokens becomes a token boundary.
tok_tp = list()
tok_fp = list()
tok_tn = list()
tok_fn = list()


for i, event in enumerate(events):

  if event['predicted_outcome'] == event['true_outcome']:
    eff_success.append(i)

  if ('0:%MAY_BREAK_SENTENCE', 1.0) in event['features']:
    if event['predicted_outcome'] == 'BREAK_SENTENCE':
      if event['true_outcome'] == 'BREAK_SENTENCE':
        seg_tp.append(i)
      else:
        seg_fp.append(i)
    else:
      if event['true_outcome'] == 'BREAK_SENTENCE':
        seg_fn.append(i)
      else:
        seg_tn.append(i)

  if ('0:%MAY_SPLIT', 1.0) in event['features']\
   or ('0:%MAY_JOIN', 1.0) in event['features']:
    if event['predicted_outcome'] == 'JOIN':
      if event['true_outcome'] == 'JOIN':
        tok_tn.append(i)
      else:
        tok_fn.append(i)
    else:
      if event['true_outcome'] == 'JOIN':
        tok_fp.append(i)
      else:
        tok_tp.append(i)


eff_tot = len(events)
eff_acc = len(eff_success) / float(eff_tot) if eff_tot > 0 else 'no data'

 
seg_tot = len(seg_tp) + len(seg_tn) + len(seg_fp) + len(seg_fn)
seg_acc = (len(seg_tp) + len(seg_tn)) / float(seg_tot) \
            if seg_tot > 0 else 'no data'

seg_true_t = len(seg_tp) + len(seg_fp)
seg_prec = len(seg_tp) / float(seg_true_t) if seg_true_t > 0 else 'no data'

seg_pred_t = len(seg_tp) + len(seg_fn)
seg_rec = len(seg_tp) / float(seg_pred_t) if seg_pred_t > 0 else 'no data'

seg_fm = 2 * (seg_prec * seg_rec) / float(seg_prec + seg_rec) \
          if seg_prec != 'no data' and seg_rec != 'no data' \
          and seg_prec + seg_rec > 0  else 'no data'


tok_tot = len(tok_tp) + len(tok_tn) + len(tok_fp) + len(tok_fn)
tok_acc = (len(tok_tp) + len(tok_tn)) / float(tok_tot) \
            if tok_tot > 0 else 'no data'

tok_true_t = len(tok_tp) + len(tok_fp)
tok_prec = len(tok_tp) / float(tok_true_t) if tok_true_t > 0 else 'no data'

tok_pred_t = len(tok_tp) + len(tok_fn)
tok_rec = len(tok_tp) / float(tok_pred_t) if tok_pred_t > 0 else 'no data'

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
