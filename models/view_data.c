#include "view_data.h"
#include <stdlib.h>
#include <string.h>

void wena_view_data_free(WenaViewData *data)
{
    if (data == NULL) return;
    free(data->cards);
    free(data->lists);
    free(data->swimlanes);
    free(data->labels);
    free(data->users);
    free(data->activities);
    free(data->changes);
    free(data->change_dependencies);
    memset(data, 0, sizeof(*data));
}

const char *wena_view_user_name(const WenaViewData *data, const char *id)
{
    size_t i;
    if (data == NULL || id == NULL) return "";
    for (i = 0; i < data->user_count; ++i)
        if (!strcmp(data->users[i].id, id)) return data->users[i].name[0] ? data->users[i].name : id;
    return id;
}

const WenaViewList *wena_view_list(const WenaViewData *data, const char *id)
{
    size_t i;
    if (data == NULL || id == NULL) return NULL;
    for (i = 0; i < data->list_count; ++i)
        if (!strcmp(data->lists[i].id, id)) return &data->lists[i];
    return NULL;
}

const WenaViewCard *wena_view_card(const WenaViewData *data, const char *id)
{
    size_t i;
    if (data == NULL || id == NULL) return NULL;
    for (i = 0; i < data->card_count; ++i)
        if (!strcmp(data->cards[i].id, id)) return &data->cards[i];
    return NULL;
}

const WenaViewLabel *wena_view_label(const WenaViewData *data, const char *id)
{
    size_t i;
    if (data == NULL || id == NULL) return NULL;
    for (i = 0; i < data->label_count; ++i)
        if (!strcmp(data->labels[i].id, id)) return &data->labels[i];
    return NULL;
}
