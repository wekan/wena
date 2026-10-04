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
    free(data->custom_fields);
    free(data->boards);
    free(data->field_items);
    free(data->sprints);
    free(data->releases);
    free(data->events);
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

const char *wena_view_field_value(const WenaViewData *data, const WenaViewCard *card, const char *field_id)
{
    size_t i, j;
    if (data == NULL || card == NULL || field_id == NULL) return "";
    for (i = 0; i < card->field_count; ++i) {
        if (strcmp(card->fields[i].field_id, field_id)) continue;
        for (j = 0; j < data->field_item_count; ++j)
            if (!strcmp(data->field_items[j].field_id, field_id) && !strcmp(data->field_items[j].item_id, card->fields[i].value))
                return data->field_items[j].name;
        return card->fields[i].value;
    }
    return "";
}

const WenaViewLabel *wena_view_label(const WenaViewData *data, const char *id)
{
    size_t i;
    if (data == NULL || id == NULL) return NULL;
    for (i = 0; i < data->label_count; ++i)
        if (!strcmp(data->labels[i].id, id)) return &data->labels[i];
    return NULL;
}
