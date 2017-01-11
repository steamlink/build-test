# -*- coding: utf-8 -*-

"""
    eve-demo settings
    ~~~~~~~~~~~~~~~~~

    Settings file for our little demo.

    PLEASE NOTE: We don't need to create the two collections in MongoDB.
    Actually, we don't even need to create the database: GET requests on an
    empty/non-existant DB will be served correctly ('200' OK with an empty
    collection); DELETE/PATCH will receive appropriate responses ('404' Not
    Found), and POST requests will create database and collections when needed.
    Keep in mind however that such an auto-managed database will most likely
    perform poorly since it lacks any sort of optimized index.

    :copyright: (c) 2016 by Nicola Iarocci.
    :license: BSD, see LICENSE for more details.
"""

import os

URL_PREFIX = 'api'
API_VERSION = 'v0'
# We want to seamlessy run our API both locally and on Heroku. If running on
# Heroku, sensible DB connection settings are stored in environment variables.
MONGO_HOST = os.environ.get('MONGO_HOST', 'localhost')
MONGO_PORT = os.environ.get('MONGO_PORT', 27017)
# MONGO_USERNAME = os.environ.get('MONGO_USERNAME', 'user')
# MONGO_PASSWORD = os.environ.get('MONGO_PASSWORD', 'user')
MONGO_DBNAME = os.environ.get('MONGO_DBNAME', 'evedemo')
DEBUG = True

# Enable reads (GET), inserts (POST) and DELETE for resources/collections
# (if you omit this line, the API will default to ['GET'] and provide
# read-only access to the endpoint).
RESOURCE_METHODS = ['GET', 'POST', 'DELETE']

# Enable reads (GET), edits (PATCH) and deletes of individual items
# (defaults to read-only item access).
ITEM_METHODS = ['GET', 'PATCH', 'DELETE']

# We enable standard client cache directives for all resources exposed by the
# API. We can always override these global settings later.
CACHE_CONTROL = 'max-age=20'
CACHE_EXPIRES = 20

# Our API will expose two resources (MongoDB collections): 'people' and
# 'works'. In order to allow for proper data validation, we define beaviour
# and structure.

swarms = {
    # 'title' tag in item links
    'item_title' : 'swarm',
    'schema' : {
        'swarm_name' : {
            'type' : 'string',
            'required'   : True,
            'unique'     : True
        },
        'swarm_crypto' : {
            'type' : 'dict',
            'schema' : {
                'crypto_type' : {'type' : 'string'},
                'crypto_key' : {'type' : 'string'}
                # TODO: validate key by type
            }
        }
    }
}

meshes = {
    'item_title': 'mesh',
    'schema' : {
        'mesh_name' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        },
        'radio' : {
            'type' : 'dict',
            'required' : True,
            'schema' : {
                'radio_type' : {'type' : 'string'},
                'radio_params' : {'type' : 'string'}
            }
        },
        'physical_location' : {
            'type' : 'dict',
            'schema' : {
                'location_type' : {'type' : 'string'},
                'location_params' : {'type' : 'string'}
            }
        },
        'last_allocated' : {
            'type' : 'integer'
        }
    }
}

transforms = {
    'item_title': 'transform',
    'schema' : {
        'transform_name' : {
            'type' : 'string',
            'required' : True
        },
        'active' : {
            'type' : 'string'
        },
        'filter' : {
            'type' : 'objectid',
            'required' : True,
            'data_relation' : {
                'resource' : 'filters',
                'embeddable' : True
            }
        },
        'selector' : {
            'type' : 'objectid',
            'data_relation' : {
                'resource' : 'selectors',
                'embeddable' : True
            }
        },
        'publisher' : {
            'type' : 'objectid',
            'required' : True,
            'data_relation' : {
                'resource' : 'publishers',
                'embeddable' : True
            }
        }
    }
}

filters = {
    'item_title' : 'filter',
    'schema' : {
        'filter_name' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        },
        'filter_string' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        }
    }
}

selectors = {
    'item_title' : 'selector',
    'schema' : {
        'selector_name' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        },
        'selector_string' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        }
    }
}

publishers = {
    'item_title' : 'publisher',
    'schema' : {
        'publisher_name' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        },
        'publisher_string' : {
            'type' : 'string',
            'required' : True,
            'unique' : True
        }
    }
}

nodes = {
    'item_title': 'node',
    'schema' : {
        'node_name' : {
            'type' : 'string',
            'required' : True
        },
        'swarm' : {
            'type' : 'objectid',
            'data_relation' : {
                'resource' : 'swarms',
                'embeddable' : True
            }
        },
        'mesh' : {
            'type' : 'objectid',
            'data_relation' : {
                'resource' : 'meshes',
                'embeddable' : True
            }
        },
        'token' : {
            'type' : 'string'
        },
        'sl_id' : {
            'type' : 'string'
        },
        'node_id' : {
            'type' : 'string'
        }
    }
}

sl_ids = {
    'item_title' : 'sl_id',
    'schema' : {
        'id' : {
            'type' : 'integer',
            'required' : True,
            'unique' : True
        }
    }
}

DOMAIN = {
    'swarms' : swarms,
    'meshes' : meshes,
    'transforms' : transforms,
    'filters' : filters,
    'selectors' : selectors,
    'publishers' : publishers,
    'nodes' : nodes
}

