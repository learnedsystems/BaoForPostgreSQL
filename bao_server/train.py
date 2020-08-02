import storage
import model
import os
import shutil
import reg_blocker

class BaoTrainingException(Exception):
    pass

def train_and_swap(fn, old, tmp, verbose=False):
    if os.path.exists(fn):
        old_model = model.BaoRegression(have_cache_data=True)
        old_model.load(fn)
    else:
        old_model = None

    new_model = train_and_save_model(tmp, verbose=verbose)
    max_retries = 5
    current_retry = 1
    while not reg_blocker.should_replace_model(old_model, new_model):
        if current_retry >= max_retries == 0:
            print("Could not train model with better regression profile.")
            return
        
        print("New model rejected when compared with old model. "
              + "Trying to retrain with emphasis on regressions.")
        print("Retry #", current_retry)
        new_model = train_and_save_model(tmp, verbose=verbose,
                                         emphasize_experiments=current_retry)
        current_retry += 1

    if os.path.exists(fn):
        shutil.rmtree(old, ignore_errors=True)
        os.rename(fn, old)
    os.rename(tmp, fn)

def train_and_save_model(fn, verbose=True, emphasize_experiments=0):
    all_experience = storage.experience()

    for _ in range(emphasize_experiments):
        all_experience.extend(storage.experiment_experience())
    
    x = [i[0] for i in all_experience]
    y = [i[1] for i in all_experience]        
    
    if not all_experience:
        raise BaoTrainingException("Cannot train a Bao model with no experience")
    
    if len(all_experience) < 20:
        print("Warning: trying to train a Bao model with fewer than 20 datapoints.")

    reg = model.BaoRegression(have_cache_data=True, verbose=verbose)
    reg.fit(x, y)
    reg.save(fn)
    return reg


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: train.py MODEL_FILE")
        exit(-1)
    train_and_save_model(sys.argv[1])

    print("Model saved, attempting load...")
    reg = model.BaoRegression(have_cache_data=True)
    reg.load(sys.argv[1])

