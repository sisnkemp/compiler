struct element_type {
        char *et_name;
        int et_type;
};

const struct element_type elements[] = {
        { "drive", 3 },
        { "picker", 0 },
        { "portal", 2 },
        { "slot", 1 },
        { 0L, 0 },
};
