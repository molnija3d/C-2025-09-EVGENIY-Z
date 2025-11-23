#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

// Disable deprecation warnings for GTK TreeView
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Simple struct for file info
typedef struct {
    gchar *name;
    gchar *type;
    gchar *size;
    gboolean is_directory;
} FileInfo;

// Tree model columns
enum {
    COL_NAME = 0,
    COL_TYPE,
    COL_SIZE,
    COL_IS_DIRECTORY,
    NUM_COLS
};

// Recursive function to scan directory
void scan_directory(const gchar *path, GtkTreeStore *store, GtkTreeIter *parent) {
    GDir *dir;
    const gchar *filename;
    GError *error = NULL;

    dir = g_dir_open(path, 0, &error);
    if (!dir) {
        g_printerr("Error opening directory: %s\n", error->message);
        g_error_free(error);
        return;
    }

    while ((filename = g_dir_read_name(dir))) {

        gchar *full_path = g_build_filename(path, filename, NULL);
        gboolean is_dir = g_file_test(full_path, G_FILE_TEST_IS_DIR);
        
        gchar *size_str = g_strdup("");
        if (!is_dir) {
            struct stat st;
            if (g_stat(full_path, &st) == 0) {
                if (st.st_size < 1024) {
                    size_str = g_strdup_printf("%ld B", st.st_size);
                } else if (st.st_size < 1024 * 1024) {
                    size_str = g_strdup_printf("%.1f KB", st.st_size / 1024.0);
                } else {
                    size_str = g_strdup_printf("%.1f MB", st.st_size / (1024.0 * 1024.0));
                }
            } else {
                size_str = g_strdup("Unknown");
            }
        }

        // Add to tree store
        GtkTreeIter iter;
        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
                          COL_NAME, filename,
                          COL_TYPE, is_dir ? "Directory" : "File",
                          COL_SIZE, size_str,
                          COL_IS_DIRECTORY, is_dir,
                          -1);

        // Recursive call for subdirectories
        if (is_dir) {
            scan_directory(full_path, store, &iter);
        }

        g_free(full_path);
        g_free(size_str);
    }

    g_dir_close(dir);
}

// Create and fill the tree model
GtkTreeModel* create_and_fill_model(void) {
    GtkTreeStore *store = gtk_tree_store_new(NUM_COLS,
                                            G_TYPE_STRING,  // name
                                            G_TYPE_STRING,  // type
                                            G_TYPE_STRING,  // size
                                            G_TYPE_BOOLEAN); // is_directory

    gchar *cwd = g_get_current_dir();
    GtkTreeIter parent;
    
    // Add root element
    gtk_tree_store_append(store, &parent, NULL);
    gtk_tree_store_set(store, &parent,
                      COL_NAME, cwd,
                      COL_TYPE, "Root Directory",
                      COL_SIZE, "",
                      COL_IS_DIRECTORY, TRUE,
                      -1);

    // Fill with data
    scan_directory(cwd, store, &parent);
    g_free(cwd);

    return GTK_TREE_MODEL(store);
}

// Setup the tree view columns
void setup_tree_view(GtkWidget *tree_view) {
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    // Name column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_expand(column, TRUE);

    // Type column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", COL_TYPE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_min_width(column, 120);

    // Size column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Size", renderer, "text", COL_SIZE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_min_width(column, 100);
}

// Activation function
static void activate(GtkApplication* app) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    GtkTreeModel *model;

    // Create main window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Simple File Browser");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Create scrolled window
    scrolled_window = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window), scrolled_window);

    // Create tree view
    tree_view = gtk_tree_view_new();
    
    // Create and set model
    model = create_and_fill_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), model);
    g_object_unref(model);

    // Setup columns
    setup_tree_view(tree_view);

    // Expand all rows by default
    gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_view));

    // Add tree view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), tree_view);

    // Show window
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.example.filebrowser", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
