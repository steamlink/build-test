apiPrefixUrl = "/api/v0";
idSelector = "#";
debug_flag = true;

debug = function(something) {
    if (debug_flag) {
        console.log(something);
    }
}

getFromServer = function(resource, callback) {
    $.ajax({
        url: apiPrefixUrl.concat(resource),
        type: "GET",
        success: function (data) {
            if (callback) {
                callback(data);
            }
        },
        error: function (xhr, ajaxOptions, thrownError) {
            console.log(xhr.status);
            console.log(thrownError);
        }
    });
}

postToServer = function(resource, value, callback) {
    $.ajax({
        type: "POST",
        url: apiPrefixUrl.concat(resource),
        data: JSON.stringify(value),
        contentType: "application/json; charset=utf-8",
        dataType: "json",
        success: function(result) {
            callback(result);
        },
        failure: function(errMsg) {
            alert(errMsg);
        }
    });
}


insertResourceInListClass = function(arr, className) {

    for (i=0; i < arr._items.length; i++){
        $("."+className).append("<tr>");
        $("."+className).append("<td>" + arr._items[i].mesh_name + "</td>");
        $("."+className).append("<td>" + arr._items[i].radio.radio_type + "</td>");
        $("."+className).append("<td>" + arr._items[i].radio.radio_params + "</td>");
        $("."+className).append("<td>" + arr._items[i].physical_location.location_params + "</td>");
        $("."+className).append("</tr>");
    }
}

printMessage = function(msg) {
    console.log(msg);
}

insertInMeshList = function(resource) {
    insertResourceInListClass(resource, "mesh_list");
}

insertInSwarmList = function(resource) {
    insertResourceInListClass(resource, "swarm_list");
}

insertInNodeList = function(resource) {
    insertResourceInListClass(resource, "node_list");
}

insertInTransformList = function(resource) {
    insertResourceInListClass(resource, "transform_list");
}

getFromServer("/meshes", insertInMeshList);
getFromServer("/swarms", insertInSwarmList);
//getFromServer("/nodes", insertInNodeList);
//getFromServer("/transforms", insertInTransformsList);





submitForm = function(form_name, resource) {
    var toSend = $(idSelector.concat(form_name)).serializeObject();
    console.log(toSend);
    $.ajax({
        type: "POST",
        url: apiPrefixUrl.concat(resource),
        data: JSON.stringify(toSend),
        contentType: "application/json; charset=utf-8",
        dataType: "json",
        success: function(data) {
            console.log(data);
        },
        failure: function(errMsg) {
            alert(errMsg);
        }
    });
}

/* TEST DATA */

window.onload = function () {

    getFromServer("/filters")

    app_transforms = new Vue({
        el: '#app_transforms',
        data: {
            filters : [],
            selectors : [],
            publishers : [],
            form_transform : {
                name : "",
                filter : {
                    filter_name : "",
                    filter_string : ""
                },
                selector : {
                    selector_name : "",
                    selector_string : ""
                },
                publisher : {
                    publisher_name : "",
                    publisher_string : ""
                }
            },
            new_transform : {
                name : "",
                active : "",
                filter : "",
                selector : "",
                publisher : ""
            },
            filter_picked : "",
            custom_filter : "",
            selector_picked : "",
            custom_selector : "",
            publisher_picked : "",
            custom_publisher : ""
        },
        mounted : function () {
            console.log("app ready!");
            this.fetchAll();
        },
        methods: {
            submitFilter: function() {
                var self = this;
                postToServer("/filters", this.form_transform.filter, function(result) {
                    self.new_transform.filter = result._id;
                    self.form_transform.filter._id = result._id;
                    self.filters.push(self.form_transform.filter);
                    console.log(self.filters);
                })
            },
            submitSelector: function() {
                var self = this;
                postToServer("/selectors", this.form_transform.selector, function(result) {
                    self.new_transform.selector = result._id;
                    self.form_transform.selector._id = result._id;
                    self.selectors.push(self.form_transform.selector);
                    console.log(self.selectors);
                })
                console.log("submitting selector");
            },
            submitPublisher: function() {
                var self = this;
                postToServer("/publishers", this.form_transform.publisher, function(result) {
                    self.new_transform.publisher = result._id;
                    self.form_transform.publisher._id = result._id;
                    self.publishers.push(self.form_transform.publisher);
                    console.log(self.publishers);
                })
                console.log("submitting publisher");
            },
            submitTransform : function () {
                console.log("submitting transform");
                console.log(this.new_transform);


            },
            fetchAll : function () {
                var self = this;
                getFromServer('/filters', function(result) {
                    self.filters = result._items;
                });
                getFromServer('/selectors', function(result) {
                    self.selector = result._items;
                });
                getFromServer('/publishers', function(result) {
                    self.publishers = result._items;
                });
                console.log("fetching transforms");
            }
        }
    })
}



