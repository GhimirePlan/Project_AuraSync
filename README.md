# AuraSync

Privacy-preserving acoustic comfort estimation using edge DSP, a three-class
INT8 DS-CNN, and an ESP32-S3 dashboard.

## Pipeline

1. Audit the fixed training, validation, and test splits in
   [the dataset repository](https://github.com/GhimirePlan/AS_Final_Dataset_).
2. Run [the training notebook](AS_DL_Model/AuraSync_CNN_Training.ipynb) for
   preprocessing, teacher training, student distillation, and INT8 evaluation.
3. Use [the deployment package](AS_DL_Model/Deployed_Package%20%26%20Training_Logs/)
   for exported models, coefficients, metrics, and audit reports.
4. Build and upload [the firmware](AuraSync_Firmware/) using its
   [upload guide](AuraSync_Firmware/UPLOAD_GUIDE.md).
5. Consult [the project documentation](Documentations/) for the report and slides.

The dataset is pinned as a Git submodule. After cloning this project, run
`git submodule update --init AS_Dataset_` to retrieve it. Firmware files are
tracked directly in this repository; the existing local firmware repository
and its history have not been changed.

## Reconstructed commit history

This repository organizes an existing project snapshot into exactly 40 commits,
ordered by pipeline stage and dated July 14 through October 13, 2026, using the
Nepal time zone (UTC+05:45). These are reconstructed organizational dates, not
evidence of when the original development, training, or testing occurred.
Intermediate commits incrementally introduce pipeline sections; the final
snapshot preserves the supplied project files.

The large demonstration video is intentionally excluded from Git. Existing
artifact reports and test results are included as supplied; organizing this
history does not rerun training or certify hardware operation.
