apiPrefixUrl = "/api/v0";
idSelector = "#";

getFromServer = function(resource, callback) {
    console.log('second');
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

updateField = function(resource, value, etag, callback) {
    $.ajax({
        type: "PATCH",
        url: apiPrefixUrl.concat(resource),
        data: JSON.stringify(value),
        contentType: "application/json; charset=utf-8",
        dataType: "json",
        headers: {
            "If-Match" : etag
        },
        success: function(result) {
            callback(result);
        },
        failure: function(errMsg) {
            alert(errMsg);
        }
    });
}

postToServer = function(resource, value, callback) {
    console.log('first');
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

window.onload = function () {

    getFromServer("/filters")

    app_transforms = new Vue({
        el: '#app_transforms',
        data: {
            filters : [],
            selectors : [],
            publishers : [],
            transforms : [],
            meshes : [],
            swarms : [],
            nodes : [],
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
                transform_name : "",
                active : "",
                filter : "",
                selector : "",
                publisher : ""
            },
            new_node : {
                selected_swarm : "",
                selected_mesh : "",
                node_name : "",
                token : "",
            },
            new_mesh : {
                mesh_name : "",
                radio : {
                    radio_type : "",
                    radio_params : ""
                },
                physical_location : {
                    location_params : ""
                }
            },
            new_swarm : {
                swarm_name : "",
                swarm_crypto : {
                    crypto_type : "aes",
                    crypto_key : ""
                }
            },
            filter_picked : -1,
            custom_filter : "",
            selector_picked : -1,
            custom_selector : "",
            publisher_picked : -1,
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
                this.new_transform.transform_name = this.form_transform.name;
                this.new_transform.active = "off";
                if (this.filter_picked != -1){
                    this.new_transform.filter = this.filters[this.filter_picked]._id;
                }
                if (this.selector_picked != -1){
                    this.new_transform.selector = this.selectors[this.selector_picked]._id;
                }
                if (this.publisher_picked != -1){
                    this.new_transform.publisher = this.publishers[this.publisher_picked]._id;
                }
                if((this.filter_picked != -1) && (this.publisher_picked != -1)) {
                    var self = this;
                    postToServer("/transforms", this.new_transform, function (result) {
                        console.log(result);
                        console.log("transform added");
                    })
                }
                console.log(this.new_transform);
            },
            activateTransform : function (index) {
                var new_state = "";
                console.log("activating transform" + index);
                if (this.transforms[index].active === "off") {
                    new_state = "on"
                } else {
                    new_state = "off";
                }
                var resource = "/" + this.transforms[index]._links.self.href;
                console.log(resource);
                var update = {
                    'active' : new_state
                };
                var self = this;
                updateField(resource, update, this.transforms[index]._etag, function(result) {
                    self.transforms[index].active = new_state;
                    self.transforms[index]._etag = result._etag;
                    console.log(result);
                });

            },
            addMesh : function () {
                var self = this;
                postToServer("/meshes", this.new_mesh, function(result) {
                    self.meshes.push(self.new_mesh);
                    console.log(self.new_mesh);
                });
            },
            addSwarm : function () {
                var self = this;
                postToServer("/swarms", this.new_swarm, function(result) {
                    self.swarms.push(self.new_swarm);
                    console.log(self.new_swarm);
                });
            },
            addNode : function () {
                var self = this;
                var nodeToSend = {
                    node_name : self.new_node.node_name,
                    swarm : self.swarms[self.new_node.selected_swarm]._id,
                    mesh : self.meshes[self.new_node.selected_mesh]._id
                };
                postToServer("/nodes", nodeToSend, self.fetchNodes);

            },
            fetchNodes : function () {
                var self = this;
                setTimeout(function() {
                    console.log("fetching new nodes");
                    getFromServer('/nodes?embedded={"swarm":1,"mesh":1}', function(result) {
                        console.log(self);
                        self.nodes = result._items;
                    });
                }, 1000)
            },
            fetchAll : function () {
                var self = this;
                getFromServer('/filters', function(result) {
                    self.filters = result._items;
                });
                getFromServer('/selectors', function(result) {
                    self.selectors = result._items;
                });
                getFromServer('/publishers', function(result) {
                    self.publishers = result._items;
                });
                getFromServer('/transforms?embedded={"filter":1,"selector":1,"publisher":1}', function(result) {
                    self.transforms = result._items;
                });
                getFromServer('/meshes', function(result) {
                    self.meshes = result._items;
                });
                getFromServer('/swarms', function(result) {
                    self.swarms = result._items;
                });
                getFromServer('/nodes?embedded={"swarm":1,"mesh":1}', function(result) {
                    self.nodes = result._items;
                });
                console.log("fetching all from servers");
            }
        }
    })
}



