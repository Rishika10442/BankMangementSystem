#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "loan_model.h"
#include "manager_model.h"
#include "bank_model.h"
#include "feedback_model.h"

#define FEEDBACK_DATA_FILE "/home/rishika-gupta/ssProject/feedback_data.txt"


// Function to get the last feedback ID
int get_last_feedback_id() {
    int last_id = 0;
    FILE *file = fopen(FEEDBACK_DATA_FILE, "r");
    if (file == NULL) {
        return 0;  // Start with ID 1 if the file doesn't exist
    }
     Feedback temp_feedback;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        int current_id;

        // Try to parse the feedback ID (the fifth field in the line)
        // Parse the feedback ID (the fifth field in the line)
        if (sscanf(line, "%[^,],%[^,],%d,%d,%d", temp_feedback.desc, temp_feedback.review, 
                   &temp_feedback.custID, &temp_feedback.manID, &current_id) == 5) {
            printf("Debug: Successfully parsed feedback ID: %d from line: '%s'\n", current_id, line);
            if (current_id > last_id) {
                last_id = current_id;
            }
        } else {
            printf("Debug: Failed to parse feedback ID from line: '%s'\n", line);
        }
    }

    fclose(file);
    printf("Debug: Last feedback ID found: %d\n", last_id);
    return last_id;
}

// Function to log feedback to the file (with file locking)
int log_feedback(Feedback* feedback) {
    int fd = open(FEEDBACK_DATA_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if (fd == -1) {
        perror("Error opening feedback file");
        return 0;
    }

    // // Acquire a lock on the file
    // struct flock lock;
    // memset(&lock, 0, sizeof(lock));
    // lock.l_type = F_WRLCK;  // Write lock

    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;  // Write lock
    lock.l_whence = SEEK_END;  // Locking at the end for appending
    lock.l_start = 0;  // Start of the file (if appending, we start at the current end)
    lock.l_len = sizeof(Feedback);

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error acquiring file lock");
        close(fd);
        return 0;
    }

    // Log the feedback data to the file
    FILE *file = fdopen(fd, "a");
    if (file) {
        fprintf(file, "%s,%s,%d,%d,%d\n",
                feedback->desc,
                feedback->review,
                feedback->custID,
                feedback->manID,
                feedback->feedbackID);
        fflush(file);
        fclose(file);
    }


    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("Error releasing file lock");
    }
    // Release the lock and close the file
    close(fd);
    return 1;
}

// Function to enter a feedback
int enter_feedback(const char *subject, int custID) {
    Feedback new_feedback;

    // Initialize feedback details
    strcpy(new_feedback.desc, subject);
    strcpy(new_feedback.review, "none");  // Initialize review as empty
    new_feedback.custID = custID;
    new_feedback.manID = -1;          // Default manager ID (no specific manager yet)
    new_feedback.feedbackID = get_last_feedback_id() + 1;  // Increment feedback ID

    // Save feedback to file
    if (log_feedback(&new_feedback)) {
        printf("Feedback submitted successfully. Feedback ID: %d\n", new_feedback.feedbackID);
        return 1;
    }

    return 0;
}

// Function to retrieve feedback history for a specific customer
int feedback_history(int custID, Feedback feedbacks[], int max_feedbacks) {
    FILE *file = fopen(FEEDBACK_DATA_FILE, "r");
    if (file == NULL) {
        perror("Failed to open feedback data file");
        return -1;  // Error case
    }

    char line[256];
    int feedback_count = 0;

    while (fgets(line, sizeof(line), file)) {
        Feedback temp_feedback;

        // Parse the feedback data from the file
        if (sscanf(line, "%[^,],%[^,],%d,%d,%d", 
                   temp_feedback.desc, temp_feedback.review, 
                   &temp_feedback.custID, &temp_feedback.manID, 
                   &temp_feedback.feedbackID) == 5) {
            
            // Check if the customer ID matches
            if (temp_feedback.custID == custID) {
                if (feedback_count < max_feedbacks) {
                    feedbacks[feedback_count++] = temp_feedback;  // Add to the array
                }
            }
        } else {
            printf("Debug: Failed to parse feedback record from line: '%s'\n", line);
        }
    }

    fclose(file);
    return feedback_count;  // Return the number of feedbacks found
}

// Function to enter a review for an existing feedback
int enter_feedbackReview(int feedbackID, const char *review, int manID) {
    Feedback feedbacks[MAX_FEEDBACKS];
    int num_feedbacks = load_all_feedbacks(feedbacks, MAX_FEEDBACKS);
    int feedback_found = 0;

    for (int i = 0; i < num_feedbacks; i++) {
        if (feedbacks[i].feedbackID == feedbackID) {
            // Update the review and manager ID for the matching feedback
            strcpy(feedbacks[i].review, review);
            feedbacks[i].manID = manID;

            feedback_found = 1;
            break;
        }
    }

    if (!feedback_found) {
        printf("Feedback with ID %d not found.\n", feedbackID);
        return -1;  // Feedback not found
    }

    // Save the updated feedbacks back to the file
    if (save_all_feedbacks(feedbacks, num_feedbacks)) {
        printf("Feedback review updated successfully.\n");
        return 1;  // Success
    } else {
        printf("Failed to update feedback review.\n");
        return 0;  // Failure
    }
}

// Function to save all feedbacks back to the file (with file locking)
int save_all_feedbacks(Feedback feedbacks[], int num_feedbacks) {
    int fd = open(FEEDBACK_DATA_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    
    if (fd == -1) {
        perror("Error opening feedback file");
        return 0;
    }

    // Acquire a lock on the file
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;  // Write lock

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error acquiring file lock");
        close(fd);
        return 0;
    }

    // Write all feedbacks to the file
    FILE *file = fdopen(fd, "w");
    if (file) {
        for (int i = 0; i < num_feedbacks; i++) {
            fprintf(file, "%s,%s,%d,%d,%d\n",
                    feedbacks[i].desc,
                    feedbacks[i].review,
                    feedbacks[i].custID,
                    feedbacks[i].manID,
                    feedbacks[i].feedbackID);
        }
        fclose(file);
    }

    // Release the lock and close the file
    close(fd);
    return 1;
}

// Function to load all feedbacks from the file into an array
int load_all_feedbacks(Feedback feedbacks[], int max_feedbacks) {
    int fd = open(FEEDBACK_DATA_FILE, O_RDONLY);
    
    if (fd == -1) {
        perror("Error opening feedback file");
        return -1;
    }

    // Acquire a read lock on the file
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;  // Read lock

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error acquiring file lock");
        close(fd);
        return -1;
    }

    // Read the feedback data
    FILE *file = fdopen(fd, "r");
    if (file == NULL) {
        perror("Error opening file stream");
        close(fd);
        return -1;
    }

    char line[256];
    int feedback_count = 0;

    while (fgets(line, sizeof(line), file) && feedback_count < max_feedbacks) {
        Feedback temp_feedback;

        // Parse the feedback data from the file
        if (sscanf(line, "%[^,],%[^,],%d,%d,%d", temp_feedback.desc, temp_feedback.review,
                   &temp_feedback.custID, &temp_feedback.manID, &temp_feedback.feedbackID) == 5) {
            feedbacks[feedback_count++] = temp_feedback;  // Add to the array
        } else {
            printf("Failed to parse feedback from line: '%s'\n", line);
        }
    }

    fclose(file);

    // Release the lock and close the file
    close(fd);
    return feedback_count;  // Return the number of feedbacks loaded
}

// Function to retrieve feedback history for a specific manager
int feedback_history_by_manager(int manID, Feedback feedbacks[], int max_feedbacks) {
    FILE *file = fopen(FEEDBACK_DATA_FILE, "r");
    if (file == NULL) {
        perror("Failed to open feedback data file");
        return -1;  // Error case
    }

    char line[256];
    int feedback_count = 0;

    while (fgets(line, sizeof(line), file)) {
        Feedback temp_feedback;

        // Parse the feedback data from the file
        if (sscanf(line, "%[^,],%[^,],%d,%d,%d", 
                   temp_feedback.desc, temp_feedback.review, 
                   &temp_feedback.custID, &temp_feedback.manID, 
                   &temp_feedback.feedbackID) == 5) {
            
            // Check if the manager ID matches
            if (temp_feedback.manID == manID) {
                if (feedback_count < max_feedbacks) {
                    feedbacks[feedback_count++] = temp_feedback;  // Add to the array
                }
            }
        } else {
            printf("Debug: Failed to parse feedback record from line: '%s'\n", line);
        }
    }

    fclose(file);
    return feedback_count;  // Return the number of feedbacks found
}
