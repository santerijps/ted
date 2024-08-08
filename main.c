#include <conio.h>
#include <stdio.h>

#include "clib/String.h"
#include "clib/Terminal.h"

typedef struct TextArea {
    String text;    // Text content
    int row_count;  // Number of rows
    int row, col;   // Cursor row and column
    size_t index;   // Cursor index
    int multiline;  // Does the TextArea support multiple lines?
} TextArea;

TextArea textarea_make(size_t capacity, int multiline) {
    return (TextArea) {
        .text = string_make(capacity),
        .row_count = 1,
        .row = 1,
        .col = 1,
        .index = 0,
        .multiline = multiline,
    };
}

void textarea_release(TextArea *ta) {
    string_release(&ta->text);
}

size_t textarea_index(TextArea *ta) {
    int row = 1, col = 1;
    size_t i = 0;
    do {
        if (row == ta->row && col == ta->col) {
            break;
        }
        if (ta->text.ptr[i] == '\n') {
            row += 1;
            col = 1;
        } else {
            col += 1;
        }
        i += 1;
    } while (i < ta->text.len);
    return i;
}

size_t textarea_col_count(TextArea *ta, int target_row) {
    int row = 1, col_count = 0;
    size_t i = 0;
    do {
        if (row == target_row) {
            for (size_t j = i; j < ta->text.len && ta->text.ptr[j] != '\n'; j += 1) {
                col_count += 1;
            }
            break;
        }
        if (ta->text.ptr[i] == '\n') {
            row += 1;
        }
        i += 1;
    } while (i < ta->text.len);
    return col_count;
}

void textarea_type_byte(TextArea *ta, char byte) {
    if (!ta->multiline && byte == '\n') {
        return;
    }
    if ((ta->index == 0 && ta->text.len == 0) || ta->index == ta->text.len) {
        string_append_byte(&ta->text, byte);
    } else {
        string_insert_byte(&ta->text, ta->index, byte);
    }
    if (byte == '\n') {
        ta->row += 1;
        ta->col = 1;
    } else {
        ta->col += 1;
    }
    ta->index = textarea_index(ta);
    // TODO: Update row_count
}

void textarea_remove_byte(TextArea *ta) {
    if (ta->index == 0) {
        return;
    }
    char byte = ta->text.ptr[ta->index - 1];
    string_remove_byte(&ta->text, ta->index - 1);
    if (byte == '\n') {
        if (ta->row > 1) {
            ta->row -= 1;
        }
        ta->col = textarea_col_count(ta, ta->row);
    } else if (ta->col > 1) {
        ta->col -= 1;
    }
    ta->index = textarea_index(ta);
    // TODO: Update row_count
}

typedef struct State {
    FILE *file;
    TextArea *current_textarea;
    TextArea body, status;
    TerminalDimensions dimensions;
} State;

void state_init(State *state) {
    state->file = NULL;
    state->body = textarea_make(128, 1);
    state->status = textarea_make(32, 0);
    state->current_textarea = &state->body;
    state->dimensions = term_get_dimensions();
}

void state_deinit(State *state) {
    fclose(state->file);
    textarea_release(&state->body);
    textarea_release(&state->status);
}

typedef enum ControlBytes {
    BACKSPACE   = 8,
    CTRL_F      = 6,
    CTRL_O      = 15,
    CTRL_Q      = 17,
    CTRL_S      = 19,
    CTRL_W      = 23,
} ControlBytes;

typedef enum UserCommand {
    USER_COMMAND_QUIT,
    USER_COMMAND_OPEN,
    USER_COMMAND_SAVE,
    USER_COMMAND_REMOVE_BYTE,
    USER_COMMAND_FIND,
    USER_COMMAND_CANCEL,
} UserCommand;

typedef struct UserInput {
    enum InputType {
        INPUT_TYPE_BYTE,
        INPUT_TYPE_COMMAND,
    } type;
    int byte, data;
} UserInput;

void loop(State *state);
void read_user_input(UserInput *user_input);
void update_state(State *state, UserInput *user_input);
void render_terminal(State *state);

int should_quit(UserInput *user_input);
UserCommand resolve_user_command(int byte);

int main(void) {
    State state;
    // TODO: Open the previously edited file
    // TODO: Load saved key combinations, configs etc.
    state_init(&state);

    // Setup the terminal
    term_screen_erase();

    loop(&state);
    state_deinit(&state);
    return 0;
}

void loop(State *state) {
    UserInput user_input;
    for (;;) {
        render_terminal(state);
        read_user_input(&user_input);
        if (should_quit(&user_input)) break;
        update_state(state, &user_input);
    }
}

void read_user_input(UserInput *user_input) {
    user_input->byte = getch();
    if (user_input->byte == '\r') {
        user_input->byte = '\n';
    }
    if (user_input->byte < 32 && user_input->byte != '\n') {
        user_input->type = INPUT_TYPE_COMMAND;
        user_input->data = resolve_user_command(user_input->byte);
    } else {
        user_input->type = INPUT_TYPE_BYTE;
        user_input->data = user_input->byte;
    }
}

void update_state(State *state, UserInput *user_input) {
    if (user_input->type == INPUT_TYPE_BYTE) {
        textarea_type_byte(state->current_textarea, user_input->data);
        return;
    }
    switch (user_input->data) {
        case USER_COMMAND_REMOVE_BYTE:
            textarea_remove_byte(state->current_textarea);
            break;
        case USER_COMMAND_OPEN:
            break;
    }
}

void render_terminal(State *state) {
    // Bottom status bar
    term_cursor_pos_set(state->dimensions.rows, 0);
    printf(
        "File: %llu B | Heap: %llu B | Ln: %d, Col: %d | Index: %llu",
        state->body.text.len,
        state->body.text.cap + state->status.text.cap,
        state->body.row, state->body.col,
        textarea_index(state->current_textarea)
    );
    term_line_erase_after_cursor();

    // File content
    term_cursor_pos_set_home();
    for (size_t i = 0; i < state->body.text.len; i += 1) {
        printf("%c", state->body.text.ptr[i]);
        if (state->body.text.ptr[i] == '\n') {
            term_line_erase_after_cursor();
        }
    }

    // Set cursor position
    term_cursor_pos_set(state->current_textarea->row, state->current_textarea->col);
    term_line_erase_after_cursor();
}

int should_quit(UserInput *user_input) {
    return user_input->type == INPUT_TYPE_COMMAND && user_input->data == USER_COMMAND_QUIT;
}

UserCommand resolve_user_command(int byte) {
    switch (byte) {
        case BACKSPACE: return USER_COMMAND_REMOVE_BYTE;
        case CTRL_F: return USER_COMMAND_FIND;
        case CTRL_O: return USER_COMMAND_OPEN;
        case CTRL_Q: return USER_COMMAND_QUIT;
        case CTRL_S: return USER_COMMAND_SAVE;
        default: return USER_COMMAND_CANCEL;
    }
}
